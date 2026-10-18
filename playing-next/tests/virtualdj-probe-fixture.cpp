// SPDX-License-Identifier: MIT
#include "virtualdj-fixture.hpp"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>
#include <iostream>
int wmain(int argc, wchar_t **argv) {
    if (argc != 2) return 2;
    try {
        const std::filesystem::path root(argv[1]);
        VirtualDjFixture fixture;
        auto ack = [&root](unsigned stage) {
            const auto tmp = root / "ack.tmp", final = root / "ack.txt";
            { std::ofstream out(tmp); out << stage; if (!out) throw std::runtime_error("ack write"); }
            if (!MoveFileExW(tmp.c_str(), final.c_str(), MOVEFILE_REPLACE_EXISTING)) throw std::runtime_error("ack rename");
        };
        ack(0);
        { std::ofstream ready(root / "ready.json"); ready << "{\"pid\":" << GetCurrentProcessId() << ",\"module\":" << fixture.base() << "}"; }
        unsigned current = 0;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
        while (std::chrono::steady_clock::now() < deadline) {
            if (std::filesystem::exists(root / "stop.txt")) { std::cout << "Fixture stopped normally; stage=" << current << '\n'; return current == 4 ? 0 : 3; }
            unsigned command = 0; { std::ifstream input(root / "command.txt"); input >> command; }
            if (command == current + 1 && command <= 4) {
                if (command == 1) fixture.master(4);
                if (command == 2) fixture.master(0);
                if (command == 3) for (unsigned i = 1; i <= 4; ++i) fixture.playing(i, false);
                if (command == 4) fixture.playing(2, true);
                current = command; ack(current);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        std::cerr << "Fixture timeout, not PASS\n"; return 4;
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 255; }
}
