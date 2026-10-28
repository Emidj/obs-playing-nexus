// SPDX-License-Identifier: MIT
#include "playing/providers/virtualdj-memory.hpp"
#include <algorithm>
#include <cstring>
#include <vector>

namespace playing::providers::virtualdj {
namespace {
constexpr std::uint64_t ceiling = UINT64_C(140737488355328);
bool pointer(std::uint64_t p) { return p >= 65536 && p < ceiling && p % 8 == 0; }
struct Saved { std::uint64_t address; std::vector<unsigned char> bytes; };
class Reader {
public:
    explicit Reader(Memory &memory) : memory_(memory) {}
    bool bytes(std::uint64_t address, void *out, std::size_t size) {
        if (address < 65536 || address >= ceiling || size == 0 || size > 65536
            || size >= ceiling - address || !memory_.read(address, out, size)) return false;
        const auto *begin = static_cast<unsigned char *>(out);
        saved_.push_back({address, {begin, begin + size}}); return true;
    }
    template<class T> bool integer(std::uint64_t address, T &out) {
        // Windows x64 layout is little endian; do not depend on host alignment.
        unsigned char data[sizeof(T)]{};
        if (!bytes(address, data, sizeof(data))) return false;
        out = 0;
        for (std::size_t i = 0; i < sizeof(T); ++i) out |= static_cast<T>(data[i]) << (i * 8);
        return true;
    }
    bool type(std::uint64_t object, const char *expected, const char *alternative = nullptr, bool *isAlternative = nullptr) {
        std::uint64_t table = 0, locator = 0;
        std::uint32_t signature = 0, descriptor = 0, self = 0;
        if (!pointer(object) || !integer(object, table) || !pointer(table)
            || !integer(table - 8, locator) || !pointer(locator)
            || !integer(locator, signature) || signature != 1
            || !integer(locator + 12, descriptor) || !integer(locator + 20, self)
            || self > locator) return false;
        const auto image = locator - self;
        if (!pointer(image) || descriptor > ceiling - image - 16) return false;
        char name[160]{};
        if (!bytes(image + descriptor + 16, name, sizeof(name))) return false;
        const auto end = std::find(std::begin(name), std::end(name), '\0');
        if (end == std::end(name)) return false;
        const std::string value(name, end);
        if (isAlternative) *isAlternative = alternative && value == alternative;
        return value == expected || (alternative && value == alternative);
    }
    bool text(std::uint64_t address, std::string &out) {
        if (!pointer(address)) return false;
        out.clear();
        for (std::size_t offset = 0; offset < 2048; offset += 64) {
            char chunk[64]{};
            if (!bytes(address + offset, chunk, sizeof(chunk))) return false;
            const auto end = std::find(std::begin(chunk), std::end(chunk), '\0');
            out.append(chunk, end);
            if (end != std::end(chunk)) return true;
        }
        return false;
    }
    bool coherent() {
        // Recheck every byte used, including RTTI and text at unchanged pointers.
        for (const auto &saved : saved_) {
            std::vector<unsigned char> current(saved.bytes.size());
            if (!memory_.read(saved.address, current.data(), current.size()) || current != saved.bytes) return false;
        }
        return true;
    }
private:
    Memory &memory_;
    std::vector<Saved> saved_;
};
}
Decode decode(Memory &memory, std::uint64_t module, const std::string &version)
{
    Decode result;
    auto fail = [&result](SelectionIssue issue) { result.issue = issue; result.frame = {}; return result; };
    if (version != verifiedVersion) return fail(SelectionIssue::UnverifiedVersion);
    if (!pointer(module) || module > ceiling - UINT64_C(0x4ea5210)) return fail(SelectionIssue::InvalidLayout);
    Reader reader(memory);
    auto &frame = result.frame;
    frame.version = version;
    std::uint64_t count = 0;
    if (!reader.integer(module + 0x4c1fc08, count) || (count != 2 && count != 4)
        || !reader.integer(module + 0x4c1fbf0, frame.master)) return fail(SelectionIssue::InvalidLayout);
    frame.count = static_cast<unsigned>(count);
    for (unsigned i = 0; i < frame.count; ++i) {
        auto &deck = frame.decks[i];
        std::uint32_t index = 0; unsigned char playing = 0; std::uint64_t song = 0, database = 0;
        bool fake = false;
        if (!reader.integer(module + 0x4ea51f0 + i * 8, deck.address)
            || !reader.type(deck.address, ".?AVCDeck@@")
            || !reader.integer(deck.address + 0x80, index) || index != i
            || !reader.integer(deck.address + 0xc0, playing) || playing > 1
            || !reader.integer(deck.address + 0x508, song)
            || !reader.type(song, ".?AVCSong@@", ".?AVCFakeSong@@", &fake)) return fail(SelectionIssue::InvalidDeck);
        deck.playing = playing != 0;
        if (fake) continue;
        if (!reader.integer(song + 0xa0, database)) return fail(SelectionIssue::InvalidDeck);
        if (database == 0 && !deck.playing) continue;
        std::uint64_t title = 0, artist = 0; core::Metadata track;
        if (!pointer(database) || !reader.integer(database + 0x20, title)
            || !reader.integer(database + 0x18, artist)
            || !reader.text(title, track.title) || (artist && !reader.text(artist, track.artist)))
            return fail(SelectionIssue::InvalidMetadata);
        if (!track.title.empty()) {
            if (!core::validMetadata(track)) return fail(SelectionIssue::InvalidMetadata);
            deck.track = std::move(track);
        }
    }
    if (!reader.coherent()) return fail(SelectionIssue::ChangedSnapshot);
    frame.coherent = true;
    const auto selection = select(frame);
    if (selection.issue != SelectionIssue::None) return fail(selection.issue);
    return result;
}
} // namespace playing::providers::virtualdj
