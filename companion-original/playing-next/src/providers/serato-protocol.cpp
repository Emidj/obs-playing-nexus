// SPDX-License-Identifier: MIT
// Port of obs-playing/serato-protocol.lua; MIT provenance in THIRD-PARTY-NOTICES.txt.
#include "playing/providers/serato-protocol.hpp"
#include <cstring>
#include <stdexcept>
#include <limits>
namespace playing::providers::serato {
namespace {
const std::string sentinel("\x4c\xaa\xc2\xae\x35\xb1\xc4\x76\xdb\x5a\x64\x44\x03\xbd\x41\x70", 16);
std::uint32_t integer(const std::string &data, std::size_t position) {
    if (position > data.size() || data.size() - position < 4) throw std::invalid_argument("OSC integer bounds");
    std::uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i) value = (value << 8) | static_cast<unsigned char>(data[position + i]);
    return value;
}
void integer(std::string &data, std::uint32_t value) {
    for (int i = 3; i >= 0; --i) data.push_back(static_cast<char>((value >> (i * 8)) & 255));
}
void text(std::string &data, const std::string &value) {
    if (value.size() > 8192 || value.find('\0') != std::string::npos) throw std::invalid_argument("OSC string bounds");
    data += value; data.push_back('\0'); while (data.size() % 4) data.push_back('\0');
}
std::string text(const std::string &data, std::size_t &position) {
    const auto end = data.find('\0', position);
    if (end == std::string::npos || end - position > 8192) throw std::invalid_argument("OSC text bounds");
    const auto next = (end + 4) & ~std::size_t(3);
    if (next > data.size()) throw std::invalid_argument("OSC padding bounds");
    for (auto i = end; i < next; ++i) if (data[i]) throw std::invalid_argument("OSC nonzero padding");
    const auto value = data.substr(position, end - position); position = next; return value;
}
void packet(const std::string &data, std::vector<Message> &out, unsigned depth) {
    if (depth > 8 || out.size() >= 1024 || data.size() > maxFrame || data.size() % 4) throw std::invalid_argument("OSC packet limits");
    if (data.compare(0, 8, std::string("#bundle\0", 8)) == 0) {
        if (data.size() < 16) throw std::invalid_argument("OSC bundle header");
        std::size_t position = 16;
        while (position < data.size()) {
            const auto bytes = integer(data, position); position += 4;
            if (!bytes || bytes > data.size() - position) throw std::invalid_argument("OSC bundle element");
            packet(data.substr(position, bytes), out, depth + 1); position += bytes;
        }
        return;
    }
    std::size_t position = 0; Message message; message.path = text(data, position);
    if (message.path.empty() || message.path.front() != '/') throw std::invalid_argument("OSC address");
    const auto tags = position < data.size() ? text(data, position) : ",";
    if (tags.empty() || tags.front() != ',' || tags.size() > 129) throw std::invalid_argument("OSC tags");
    for (std::size_t i = 1; i < tags.size(); ++i) {
        switch (tags[i]) {
        case 's': message.arguments.emplace_back(text(data, position)); break;
        case 'i': {
            const auto bits = integer(data, position); position += 4; std::int32_t value = 0;
            std::memcpy(&value, &bits, 4); message.arguments.emplace_back(value); break;
        }
        case 'f': {
            static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559, "OSC float32 required");
            const auto bits = integer(data, position); position += 4; float value = 0;
            std::memcpy(&value, &bits, 4); message.arguments.emplace_back(value); break;
        }
        case 'b': {
            const auto size = integer(data, position); position += 4;
            if (size > data.size() - position) throw std::invalid_argument("OSC blob");
            const auto padded = (static_cast<std::size_t>(size) + 3) & ~std::size_t(3);
            if (padded > data.size() - position) throw std::invalid_argument("OSC blob padding");
            Blob blob(data.begin() + position, data.begin() + position + size);
            for (auto j = size; j < padded; ++j) if (data[position + j]) throw std::invalid_argument("OSC blob padding bytes");
            message.arguments.emplace_back(std::move(blob)); position += padded; break;
        }
        default: throw std::invalid_argument("OSC unsupported tag");
        }
    }
    if (position != data.size()) throw std::invalid_argument("OSC trailing bytes");
    out.push_back(std::move(message));
}
}
Decoded decode(const std::string &data) {
    Decoded result;
    try { packet(data, result.messages, 0); result.valid = true; }
    catch (const std::invalid_argument &) { result.messages.clear(); }
    return result;
}
std::string encode(const Message &message) {
    if (message.path.empty() || message.path.front() != '/' || message.arguments.size() > 128) throw std::invalid_argument("OSC message");
    std::string data, tags = ","; text(data, message.path);
    for (const auto &argument : message.arguments) tags += std::holds_alternative<std::string>(argument) ? 's'
        : std::holds_alternative<float>(argument) ? 'f' : std::holds_alternative<Blob>(argument) ? 'b' : 'i';
    text(data, tags);
    for (const auto &argument : message.arguments) {
        if (const auto *value = std::get_if<std::string>(&argument)) text(data, *value);
        else if (const auto *blobValue = std::get_if<Blob>(&argument)) {
            if (blobValue->size() > maxFrame) throw std::invalid_argument("OSC blob limit");
            integer(data, static_cast<std::uint32_t>(blobValue->size())); data.append(blobValue->begin(), blobValue->end());
            while (data.size() % 4) data.push_back('\0');
        } else if (const auto *floatValue = std::get_if<float>(&argument)) {
            std::uint32_t bits = 0; std::memcpy(&bits, floatValue, 4); integer(data, bits);
        } else integer(data, static_cast<std::uint32_t>(std::get<std::int32_t>(argument)));
        if (data.size() > maxFrame) throw std::invalid_argument("OSC frame limit");
    }
    if (data.size() > maxFrame) throw std::invalid_argument("OSC frame limit");
    return data + sentinel;
}
Split split(const std::string &buffer, const std::string &incoming) {
    Split result;
    if (buffer.size() > maxFrame + sentinel.size() || incoming.size() > maxFrame + sentinel.size() - buffer.size()) return result;
    auto combined = buffer + incoming; std::size_t start = 0;
    for (;;) {
        const auto stop = combined.find(sentinel, start);
        if (stop == std::string::npos) break;
        if (result.frames.size() >= 128) return {};
        result.frames.push_back(combined.substr(start, stop - start)); start = stop + sentinel.size();
    }
    result.rest = combined.substr(start); result.valid = true; return result;
}
bool applyStatus(State &state, const Message &message, double now) {
    Event event; const auto &args = message.arguments;
    if (message.path == "/Status/Video/Mixer/Crossfader") {
        if (args.size() != 1 || !std::holds_alternative<float>(args[0])) return false;
        event.topic = Topic::Crossfader; event.values[0] = std::get<float>(args[0]); return state.apply(event, now);
    }
    if (args.empty() || !std::holds_alternative<std::int32_t>(args[0])) return false;
    const auto index = std::get<std::int32_t>(args[0]); if (index < 0 || index > 3) return false;
    event.deck = static_cast<unsigned>(index);
    if (message.path == "/Status/Deck/Song/Title" || message.path == "/Status/Deck/Song/Artist" || message.path == "/Status/Deck/Song/Filepath") {
        if (args.size() != 2 || !std::holds_alternative<std::string>(args[1])) return false;
        event.topic = message.path == "/Status/Deck/Song/Filepath"?Topic::Filepath:message.path.back() == 'e' ? Topic::Title : Topic::Artist; event.text = std::get<std::string>(args[1]);
    } else if (message.path == "/Status/Deck/Song/Valid" || message.path == "/Status/Video/Deck/Mixer/Upfader") {
        if (args.size() != 2 || !std::holds_alternative<float>(args[1])) return false;
        event.topic = message.path == "/Status/Deck/Song/Valid" ? Topic::Valid : Topic::Upfader; event.values[0] = std::get<float>(args[1]);
    } else if (message.path == "/Status/Deck/Playhead") {
        if (args.size() != 4) return false;
        event.topic = Topic::Playhead;
        for (unsigned i = 0; i < 3; ++i) { if (!std::holds_alternative<float>(args[i + 1])) return false; event.values[i] = std::get<float>(args[i + 1]); }
    } else return false;
    return state.apply(event, now);
}
}
