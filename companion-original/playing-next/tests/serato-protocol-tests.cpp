// SPDX-License-Identifier: MIT
#include "playing/providers/serato-protocol.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace playing::providers::serato;
std::string body(const Message &message) { const auto encoded = encode(message); return encoded.substr(0, encoded.size() - 16); }
std::string bundle(const std::string &element) {
    std::string out("#bundle\0", 8); out.append(8, '\0');
    for (int i = 3; i >= 0; --i) out.push_back(static_cast<char>((element.size() >> (i * 8)) & 255));
    return out + element;
}
int main() {
    unsigned checks = 0;
    auto check = [&](bool ok) { if (!ok) throw std::runtime_error("check " + std::to_string(checks + 1)); ++checks; };
    try {
        const Message mixed{"/Probe", {INT32_MIN, std::string(u8"Canción — Künstler"), Blob{97,0,98}, .375f}};
        auto decoded = decode(body(mixed)); check(decoded.valid && decoded.messages.size() == 1);
        check(std::get<std::int32_t>(decoded.messages[0].arguments[0]) == INT32_MIN);
        check(std::get<std::string>(decoded.messages[0].arguments[1]) == u8"Canción — Künstler");
        check(std::get<Blob>(decoded.messages[0].arguments[2]) == Blob({97,0,98}));
        check(std::get<float>(decoded.messages[0].arguments[3]) == .375f);
        check(encode(decoded.messages[0]) == encode(mixed));
        const auto ping = encode({"/Ping", {}});
        for (std::size_t i = 0; i <= ping.size(); ++i) {
            const auto a = split({}, ping.substr(0, i)), b = split(a.rest, ping.substr(i));
            check(a.valid && b.valid && b.rest.empty() && a.frames.size() + b.frames.size() == 1);
        }
        const auto coalesced = split({}, ping + ping + ping.substr(0, 7));
        check(coalesced.valid && coalesced.frames.size() == 2 && coalesced.rest == ping.substr(0, 7));
        check(!split({}, std::string(maxFrame + 17, 'x')).valid);
        std::string many; for (unsigned i = 0; i < 128; ++i) many += ping;
        check(split({}, many).frames.size() == 128); check(!split({}, many + ping).valid);
        check(!decode("/bad").valid); check(!decode(body({"/Ping", {}}) + std::string(4, '\0')).valid);
        auto badTags = body({"/Ping", {}}); badTags[9] = 'z'; check(!decode(badTags).valid);
        auto badPad = body({"/A", {}}); badPad[3] = 'x'; check(!decode(badPad).valid);
        check(decode(bundle(body(mixed))).messages.size() == 1);
        auto nested = body({"/Ping", {}}); for (unsigned i = 0; i < 8; ++i) nested = bundle(nested);
        check(decode(nested).valid); check(!decode(bundle(nested)).valid);
        check(!decode(std::string("#bundle\0", 8)).valid);
        auto badBundle = bundle(body(mixed)); badBundle[16] = static_cast<char>(255); check(!decode(badBundle).valid);
        check(!decode(bundle({})).valid);
        std::string large("#bundle\0", 8); large.append(8, '\0'); const auto part = bundle(body({"/Ping", {}})).substr(16);
        for (unsigned i = 0; i < 1024; ++i) large += part;
        check(decode(large).messages.size() == 1024); check(!decode(large + part).valid);
        auto blob = body({"/Blob", {Blob{1,2,3}}}); blob[12] = static_cast<char>(255); check(!decode(blob).valid);
        blob = body({"/Blob", {Blob{1,2,3}}}); blob.back() = 1; check(!decode(blob).valid);
        bool rejected = false; try { encode({"bad", {}}); } catch (const std::invalid_argument &) { rejected = true; } check(rejected);
        rejected = false; try { encode({"/Bad", {std::string(8193, 'x')}}); } catch (const std::invalid_argument &) { rejected = true; } check(rejected);
        Message args{"/Many", {}}; args.arguments.assign(128, std::int32_t(1)); check(decode(body(args)).valid);
        args.arguments.push_back(std::int32_t(1)); rejected = false;
        try { encode(args); } catch (const std::invalid_argument &) { rejected = true; } check(rejected);
        State state;
        check(applyStatus(state, {"/Status/Deck/Song/Title", {std::int32_t(0), std::string(u8"Jóga")}}, 1));
        check(applyStatus(state, {"/Status/Deck/Song/Artist", {std::int32_t(0), std::string(u8"Björk")}}, 1));
        check(applyStatus(state, {"/Status/Deck/Playhead", {std::int32_t(0), 0.f, 1.f, 120.f}}, 1));
        check(state.read(2).observation->metadata->title == u8"Jóga");
        check(applyStatus(state,{"/Status/Deck/Song/Filepath",{std::int32_t(0),std::string(u8"C:/Música/song.mp3")}},2));
        check(state.read(2.2).mediaPath==u8"C:/Música/song.mp3");
        check(!applyStatus(state,{"/Status/Deck/Song/Filepath",{std::int32_t(0),1.f}},2));
        check(!applyStatus(state, {"/Status/Deck/Song/Title", {0.f, std::string("Wrong index type")}}, 1));
        check(!applyStatus(state, {"/Status/Deck/Song/Title", {std::int32_t(4), std::string("Wrong deck")}}, 1));
        check(!applyStatus(state, {"/Status/Deck/Playhead", {std::int32_t(0), 0.f, std::numeric_limits<float>::quiet_NaN(), 120.f}}, 1));
        check(!applyStatus(state, {"/Unknown", {}}, 1));
        check(!applyStatus(state, {"/Status/Video/Mixer/Crossfader", {std::int32_t(1)}}, 1));
        check(applyStatus(state, {"/Status/Video/Mixer/Crossfader", {1.f}}, 1));
        check(state.read(2).observation->playback == playing::core::Playback::Waiting);
        std::cout << checks << " OSC/framing/status checks; no network\n"; return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
