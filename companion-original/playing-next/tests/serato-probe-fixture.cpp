// SPDX-License-Identifier: MIT
#include "serato-socket-fixture.hpp"
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>
#include <iostream>
int wmain(int argc, wchar_t **argv) {
    if (argc != 2) return 2;
    WSADATA data{}; if (WSAStartup(MAKEWORD(2,2), &data)) return 3;
    int result = 0;
    try {
        const std::filesystem::path root(argv[1]); unsigned port = 0;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(50);
        while (!port && std::chrono::steady_clock::now() < deadline) {
            { std::ifstream ready(root / "port.txt"); ready >> port; }
            if (!port) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (!port) throw std::runtime_error("fixture no OBS port");
        SeratoClient client(port); client.authorize(); bool authorized = false;
        while (!authorized && std::chrono::steady_clock::now() < deadline) {
            for (const auto &message : client.receive()) if (message.path == "/StreamMgmt/Authorize/Response") authorized = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (!authorized) throw std::runtime_error("fixture authorization"); client.pair(); client.tracks();
        auto ack = [&](unsigned stage) { const auto tmp = root / "ack.tmp", path = root / "ack.txt"; { std::ofstream out(tmp); out << stage; } if (!MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) throw std::runtime_error("fixture ack"); };
        unsigned stage = 0; ack(stage); bool done = false;
        while (std::chrono::steady_clock::now() < deadline) {
            if (std::filesystem::exists(root / "stop.txt")) { if (stage != 4) throw std::runtime_error("fixture early stop"); done = true; break; }
            unsigned command = 0; { std::ifstream input(root / "command.txt"); input >> command; }
            if (command == stage + 1 && command <= 4) {
                if (command == 1) client.sendMessage({"/Status/Video/Mixer/Crossfader", {1.f}});
                if (command == 2) for (std::int32_t i = 0; i < 2; ++i) client.sendMessage({"/Status/Deck/Playhead", {i, 0.f, 0.f, 120.f}});
                if (command == 3) { client.sendMessage({"/Status/Video/Mixer/Crossfader", {0.f}}); client.sendMessage({"/Status/Deck/Playhead", {std::int32_t(0), 0.f, 1.f, 120.f}}); }
                if (command == 4) client.sendMessage({"/StreamMgmt/Pairing/UnPair", {}});
                stage = command; ack(stage);
            }
            if (stage < 4) client.receive(); // UnPair intentionally closes the peer.
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (!done) throw std::runtime_error("fixture deadline");
        std::cout << "Serato fixture normal stop; stages=0,1,2,3,4\n";
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; result = 255; }
    WSACleanup(); return result;
}
