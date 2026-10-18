// SPDX-License-Identifier: MIT
// Audited port of obs-playing/rekordbox-live.lua, dynamic RTTI discovery.
#include "playing/providers/rekordbox-live.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>
#include <cstring>
#include <sstream>
namespace playing::providers::rekordbox {
namespace {
constexpr std::uint64_t ceiling = UINT64_C(140737488355328);
bool pointer(std::uint64_t p) { return p >= 65536 && p < ceiling && p % 8 == 0; }
std::uint32_t u32(const std::string &s, std::size_t i) {
    if (i > s.size() || s.size() - i < 4) return 0;
    std::uint32_t value = 0; for (unsigned j = 0; j < 4; ++j) value |= static_cast<std::uint32_t>(static_cast<unsigned char>(s[i + j])) << (8 * j); return value;
}
std::uint64_t u64(const std::string &s, std::size_t i = 0) {
    if (i > s.size() || s.size() - i < 8 || u32(s, i + 4) >= 32768) return 0;
    return u32(s, i) | (static_cast<std::uint64_t>(u32(s, i + 4)) << 32);
}
}
bool acceptsVersion(const std::string &version) noexcept {
    if (version.size() < 7 || version.size() > 64 || (version[0] != '6' && version[0] != '7') || version[1] != '.') return false;
    unsigned parts = 1; bool digit = false;
    for (std::size_t i = 2; i < version.size(); ++i) {
        const auto c = version[i]; if (c >= '0' && c <= '9') digit = true;
        else if (c == '.' && digit && parts < 3) { ++parts; digit = false; }
        else return false;
    }
    return parts == 3 && digit;
}
std::optional<core::Metadata> parseLabels(const std::string &text) {
    if (text.empty() || text.size() > 2048 || text.find('\0') != std::string::npos) return {};
    auto validation = text; validation.erase(std::remove_if(validation.begin(), validation.end(), [](char c) { return c == '\r' || c == '\n' || c == '\t'; }), validation.end());
    if (!core::validMetadata({validation, {}})) return {};
    std::istringstream input(text); std::string lines[3];
    for (auto &line : lines) { if (!std::getline(input, line)) return {}; if (!line.empty() && line.back() == '\r') line.pop_back(); }
    core::Metadata track; std::string *fields[3]{&track.title, &track.artist, nullptr};
    for (unsigned i = 0; i < 3; ++i) { const auto colon = lines[i].find(':'); if (colon == std::string::npos || colon == 0 || colon + 1 >= lines[i].size() || lines[i][colon + 1] != ' ') return {}; if (fields[i]) *fields[i] = lines[i].substr(colon + 2); }
    if (!core::validMetadata(track)) return {}; return track;
}
struct MemoryReader::Impl {
    virtualdj::Memory &memory; std::uint64_t module, root = 0; std::string version;
    std::map<std::uint64_t, std::string> types; std::map<std::string, std::size_t> fields;
    struct Section { std::uint32_t rva, size; }; std::vector<Section> sections;
    bool initialized = false, finished = false;
    std::size_t section = 0, chunkOffset = 0, slot = 0; std::string chunk;
    struct Activity { std::string identity; std::optional<std::int32_t> position; double time = 0; unsigned hits = 0; bool active = false; std::optional<double> lastMotion; };
    std::array<Activity, 4> activity{}; std::optional<unsigned> selected;
    struct Deck { std::optional<core::Metadata> track; std::optional<std::int32_t> position; std::uint64_t core = 0; bool master = false; std::uint64_t proofAddress = 0; std::string proof; };
    Impl(virtualdj::Memory &m, std::uint64_t base, std::string v) : memory(m), module(base), version(std::move(v)) {}
    std::string bytes(std::uint64_t address, std::size_t size) {
        if (address < 65536 || address >= ceiling || !size || size > 65536 || size >= ceiling - address) return {};
        std::string out(size, '\0'); if (!memory.read(address, out.data(), size)) return {}; return out;
    }
    std::uint64_t ptr(std::uint64_t address) { return u64(bytes(address, 8)); }
    std::string type(std::uint64_t object) {
        if (!pointer(object)) return {};
        const auto table = ptr(object); if (!pointer(table)) return {};
        const auto found = types.find(table); if (found != types.end()) return found->second;
        const auto col = ptr(table - 8); if (!pointer(col)) return {};
        const auto header = bytes(col, 24); if (header.size() != 24 || u32(header, 0) != 1 || u32(header, 20) > col) return {};
        const auto image = col - u32(header, 20); if (!pointer(image)) return {};
        const auto name = bytes(image + u32(header, 12) + 16, 160);
        const auto end = name.find('\0');
        if (end == std::string::npos || name.compare(0, 3, ".?A") != 0) return {};
        if (types.size() >= 4096) types.clear(); const auto result = name.substr(0, end); types.emplace(table, result); return result;
    }
    std::uint64_t child(std::uint64_t object, const std::string &kind, std::size_t limit) {
        if (!pointer(object)) return 0;
        const auto key = kind + ':' + std::to_string(limit), expected = ".?AV" + kind + "@@";
        const auto found = fields.find(key);
        if (found != fields.end()) { const auto p = ptr(object + found->second); if (type(p) == expected) return p; }
        const auto data = bytes(object, limit);
        for (std::size_t i = 0; i + 8 <= data.size(); i += 8) { const auto p = u64(data, i); if (type(p) == expected) { fields[key] = i; return p; } }
        return 0;
    }
    bool discover(const std::atomic<bool> &stopping) {
        if (!initialized) {
            initialized = true;
            const auto dos = bytes(module, 64); if (dos.size() != 64 || dos.compare(0, 2, "MZ") != 0) { finished = true; return false; }
            const auto pe = u32(dos, 60); if (pe > 1048576) { finished = true; return false; }
            const auto header = bytes(module + pe, 24);
            if (header.size() != 24 || header.compare(0, 6, std::string("PE\0\0\x64\x86", 6)) != 0) { finished = true; return false; }
            const auto count = static_cast<unsigned char>(header[6]) | (static_cast<unsigned char>(header[7]) << 8);
            const auto optional = static_cast<unsigned char>(header[20]) | (static_cast<unsigned char>(header[21]) << 8);
            if (!count || count > 96 || optional > 4096) { finished = true; return false; }
            const auto table = bytes(module + pe + 24 + optional, static_cast<std::size_t>(count) * 40);
            if (table.size() != static_cast<std::size_t>(count) * 40) { finished = true; return false; }
            for (unsigned i = 0; i < static_cast<unsigned>(count); ++i) {
                const auto size = u32(table, i * 40 + 8), rva = u32(table, i * 40 + 12), flags = u32(table, i * 40 + 36);
                if ((flags & 0x80000040) == 0x80000040 && size && size < 16777216) sections.push_back({rva, size});
            }
        }
        const auto started = std::chrono::steady_clock::now();
        while (section < sections.size() && !stopping) {
            const auto &s = sections[section];
            if (chunk.empty()) { chunk = bytes(module + s.rva + chunkOffset, std::min<std::size_t>(65536, s.size - chunkOffset)); slot = 0; }
            for (; slot + 8 <= chunk.size(); slot += 8) {
                if (type(u64(chunk, slot)) == ".?AVMainComponent@@") { root = module + s.rva + chunkOffset + slot; finished = true; chunk.clear(); return true; }
                if (slot % 256 == 0 && (stopping || std::chrono::steady_clock::now() - started >= std::chrono::milliseconds(4))) { slot += 8; return false; }
            }
            chunkOffset += std::min<std::size_t>(65536, s.size - chunkOffset); chunk.clear();
            if (chunkOffset >= s.size) { ++section; chunkOffset = 0; }
        }
        if (section >= sections.size()) finished = true;
        return root != 0;
    }
    std::optional<core::Metadata> track(std::uint64_t view) {
        const auto title = child(view, "TitleView@TrackInfoView@djplay", 0x200); if (!title) return {};
        for (unsigned offset = 0xd8; offset <= 0x178; offset += 8) {
            const auto p = ptr(title + offset); if (!pointer(p)) continue; std::string text;
            for (unsigned i = 0; i < 2048; i += 128) {
                const auto part = bytes(p + i, 128); if (part.empty()) break;
                const auto end = part.find('\0'); text += part.substr(0, end);
                if (end != std::string::npos) { if (const auto result = parseLabels(text)) return result; break; }
            }
        }
        return {};
    }
    bool master(std::uint64_t panel, std::uint64_t widget, Deck &deck) {
        const auto data = bytes(panel, 0x600); std::uint64_t control = 0;
        for (std::size_t i = 0; i + 8 <= data.size(); i += 8) {
            const auto p = u64(data, i);
            if (type(p) == ".?AVPlayerControllPanel@djplay@@" && child(p, "WidgetPlayer@PlayerComponent@djplay", 0x400) == widget) { control = p; break; }
        }
        const auto controls = bytes(control, 0x400);
        for (std::size_t i = 0; i + 8 <= controls.size(); i += 8) {
            const auto button = u64(controls, i); if (type(button) != ".?AVDrawableButton@juce@@" || bytes(ptr(button + 8), 7) != std::string("MASTER\0", 7)) continue;
            const auto value = child(button, "SimpleValueSource@juce", 0x400); if (!value) return false;
            deck.proofAddress = value + 0x38; deck.proof = bytes(deck.proofAddress, 8);
            if (deck.proof == std::string(8, '\0')) { deck.master = false; return true; }
            if (deck.proof == std::string("\1\0\0\0\0\0\0\0", 8)) { deck.master = true; return true; }
            deck.proof = bytes(deck.proofAddress, 9); if (deck.proof.size() != 9) return false;
            const auto kind = type(u64(deck.proof));
            if (kind == ".?AVVariantType_Void@var@juce@@") { deck.master = false; return true; }
            if (kind == ".?AVVariantType_Bool@var@juce@@" && static_cast<unsigned char>(deck.proof[8]) <= 1) { deck.master = deck.proof[8] == 1; return true; }
            return false;
        }
        return false;
    }
    Read read(double now, const std::atomic<bool> &stopping, std::optional<unsigned> manual) {
        if (stopping) return {ReadIssue::Searching, {}, {}};
        if (!acceptsVersion(version)) return {ReadIssue::Version, {}, {}};
        if (!std::isfinite(now) || now < 0 || (manual && *manual > 4)) return {ReadIssue::Unavailable, {}, {}};
        if (!root && !discover(stopping)) return {finished ? ReadIssue::Unavailable : ReadIssue::Searching, {}, {}};
        const auto main = ptr(root);
        if (type(main) != ".?AVMainComponent@@") {
            root = 0; initialized = false; finished = false; section = chunkOffset = slot = 0; chunk.clear(); sections.clear(); types.clear(); fields.clear(); activity = {}; selected.reset();
            return {ReadIssue::Searching, {}, {}};
        }
        const auto manager = child(main, "UiManager@djplay", 0x800), player = child(manager, "PlayerComponent@djplay", 0x120);
        const auto panel = child(player, "PlayerPanelL@PlayerComponent@djplay", 0x200); if (!panel) return {ReadIssue::Unavailable, {}, {}};
        std::array<std::uint64_t, 4> views{}; const auto data = bytes(panel, 0x600); bool found = false;
        for (std::size_t i = 0; i + 32 <= data.size(); i += 8) {
            found = true; for (unsigned j = 0; j < 4; ++j) { views[j] = u64(data, i + j * 8); if (type(views[j]) != ".?AVTrackInfoView@djplay@@") { found = false; break; } }
            if (found) break;
        }
        if (!found) return {ReadIssue::Unavailable, {}, {}};
        if (manual && *manual > 0) { const auto metadata = track(views[*manual - 1]); return {ReadIssue::None, manual, core::Observation{metadata ? core::Playback::Playing : core::Playback::Waiting, metadata}}; }
        std::array<Deck, 4> decks{}; unsigned masters = 0; std::optional<unsigned> chosen;
        for (unsigned i = 0; i < 4; ++i) {
            auto &deck = decks[i]; const auto widget = child(views[i], "WidgetPlayer@PlayerComponent@djplay", 0x280);
            const auto ui = child(widget, "UiPlayer@djplay", 0x40); deck.core = child(ui, "WidgetPlayer@djplay", 0x600);
            const auto position = bytes(deck.core + 0x20, 4);
            if (position.size() == 4) { const auto bits = u32(position, 0); std::int32_t value = 0; std::memcpy(&value, &bits, 4); if (value >= -3600000 && value <= 604800000) deck.position = value; }
            deck.track = track(views[i]);
            if (!widget || !master(panel, widget, deck) || (!manual && deck.track && !deck.position)) return {ReadIssue::Unavailable, {}, {}};
            if (deck.master) { ++masters; chosen = i + 1; }
        }
        for (const auto &deck : decks) if (bytes(deck.proofAddress, deck.proof.size()) != deck.proof) return {ReadIssue::ChangedSnapshot, {}, {}};
        if (masters > 1) return {ReadIssue::AmbiguousMaster, {}, {}};
        if (!manual) {
            std::array<bool, 4> active{};
            for (unsigned i = 0; i < 4; ++i) {
                const auto &deck = decks[i]; auto &previous = activity[i]; Activity next; next.time = now; next.position = deck.position;
                if (deck.track) next.identity = deck.track->title + '\0' + deck.track->artist + ':' + std::to_string(deck.core);
                if (!next.identity.empty() && next.position && previous.position && previous.identity == next.identity) {
                    const auto dt = now - previous.time;
                    if (dt >= 0 && dt < .1) next = previous;
                    else if (dt > 0 && dt <= 2) {
                        const auto delta = static_cast<std::int64_t>(*next.position) - *previous.position;
                        if (delta >= 10 && delta <= dt * 4000 + 250) { next.hits = std::min(2u, previous.hits + 1); next.lastMotion = now; next.active = previous.active || next.hits >= 2; }
                        else { next.lastMotion = previous.lastMotion; next.active = previous.active && previous.lastMotion && now - *previous.lastMotion < .65; }
                    }
                }
                previous = next; active[i] = next.active;
            }
            chosen.reset();
            for (unsigned i = 0; i < 4; ++i) if (active[i] && decks[i].master) chosen = i + 1;
            if (!chosen && selected && active[*selected - 1]) chosen = selected;
            if (!chosen) for (unsigned i = 0; i < 4; ++i) if (active[i]) { chosen = i + 1; break; }
            selected = chosen;
        } else if (!chosen) return {ReadIssue::Searching, {}, {}};
        const auto metadata = chosen ? decks[*chosen - 1].track : std::optional<core::Metadata>{};
        return {ReadIssue::None, chosen, core::Observation{metadata ? core::Playback::Playing : core::Playback::Waiting, metadata}};
    }
};
MemoryReader::MemoryReader(virtualdj::Memory &memory, std::uint64_t module, std::string version) : impl_(std::make_unique<Impl>(memory, module, std::move(version))) {}
MemoryReader::~MemoryReader() = default;
Read MemoryReader::read(double now, const std::atomic<bool> &stopping, std::optional<unsigned> manual) { return impl_->read(now, stopping, manual); }
}
