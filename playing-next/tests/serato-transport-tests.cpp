// SPDX-License-Identifier: MIT
#include "serato-socket-fixture.hpp"
#include "playing/providers/serato-transport.hpp"
#include <chrono>
#include <iostream>
#include <thread>
using namespace playing::providers;
int main(int argc, char **argv) {
    unsigned checks = 0;
    auto check = [&](bool ok) { if (!ok) throw std::runtime_error("check " + std::to_string(checks + 1)); ++checks; };
    try {
        if (argc == 2 && std::string(argv[1]) == "--bonjour") {
            serato::Transport advertised;
            advertised.start(); check(advertised.port() > 0);
            for (unsigned i = 0; i < 40; ++i) {
                const auto sample = advertised.step(i * .025);
                check(sample.kind == SampleKind::Pending);
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
            }
            advertised.stop(); check(advertised.port() == 0);
            std::cout << checks << " installed Bonjour local registration/callback/cleanup checks, no DJ peer\n";
            return 0;
        }
        serato::Transport transport(false); check(transport.port() == 0); transport.stop(); transport.start(); transport.start(); check(transport.port() > 0);
        SeratoClient client(transport.port()); check(transport.step(1).kind == SampleKind::Pending);
        client.tracks(); transport.step(1); check(transport.step(2).kind == SampleKind::Pending); // Ignore unpaired telemetry.
        client.authorize(); transport.step(2);
        std::vector<serato::Message> responses;
        for (unsigned i = 0; i < 100 && responses.empty(); ++i) { responses = client.receive(); std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
        check(responses.size() == 1 && responses[0].path == "/StreamMgmt/Authorize/Response");
        check(std::get<std::string>(responses[0].arguments[0]) == "OBS Playing");
        check(std::get<serato::Blob>(responses[0].arguments[2]).size() == 16);
        check(std::get<serato::Blob>(responses[0].arguments[2]) == serato::Blob({0x4f,0x6f,0xd5,0xd8,0xa4,0x4f,0xbc,0xc6,0x57,0x23,0x75,0xa8,0x0d,0xe9,0xa6,0x7b}));
        client.pair(); transport.step(2);
        unsigned subscriptions = 0;
        for (unsigned i = 0; i < 100 && subscriptions < 7; ++i) { for (const auto &message : client.receive()) if (message.path.compare(0,17,"/Register/Status/") == 0) ++subscriptions; std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
        check(subscriptions == 7);
        client.tracks(); transport.step(3); auto sample = transport.step(3.2);
        for (unsigned i = 0; i < 100 && !sample.observation; ++i) { std::this_thread::sleep_for(std::chrono::milliseconds(2)); sample = transport.step(3.2); }
        check(sample.observation && sample.observation->metadata && sample.observation->metadata->title == "Track 1");
        client.sendMessage({"/Status/Video/Mixer/Crossfader", {1.f}});
        for (unsigned i = 0; i < 100; ++i) { sample = transport.step(4); if (sample.observation && sample.observation->metadata && sample.observation->metadata->title == "Track 2") break; std::this_thread::sleep_for(std::chrono::milliseconds(2)); }
        check(sample.observation && sample.observation->metadata->title == "Track 2");
        for (std::int32_t i = 0; i < 2; ++i) client.sendMessage({"/Status/Deck/Playhead", {i, 0.f, 0.f, 120.f}});
        for (unsigned i = 0; i < 100; ++i) { sample = transport.step(5); if (sample.observation && !sample.observation->metadata) break; std::this_thread::sleep_for(std::chrono::milliseconds(2)); }
        check(sample.observation && sample.observation->playback == playing::core::Playback::Waiting);
        SeratoClient heartbeat(transport.port()); heartbeat.sendMessage({"/Ping", {}}); transport.step(40);
        check(transport.step(70).kind == SampleKind::Observation); // Shared heartbeat keeps silent paused stream alive.
        sample = transport.step(90); check(sample.kind == SampleKind::Unavailable && sample.issue == ProviderIssue::ConnectionLost);
        check(transport.step(91).kind == SampleKind::Pending);
        client.close(); heartbeat.close(); transport.stop(); check(transport.port() == 0); transport.stop(); transport.start(); check(transport.port() > 0);
        SeratoClient partial(transport.port()); const auto ping = serato::encode({"/Ping", {}});
        partial.bytes(ping.substr(0, 5)); transport.step(1); check(partial.receive().empty());
        partial.bytes(ping.substr(5)); transport.step(2);
        responses.clear(); for (unsigned i = 0; i < 100 && responses.empty(); ++i) { transport.step(2); responses = partial.receive(); std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
        check(responses.size() == 1 && responses[0].path == "/Ping");
        partial.close(); transport.stop();
        std::cout << checks << " local Serato transport checks; fixture peers, Bonjour advertisement disabled\n"; return 0;
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
