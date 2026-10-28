// SPDX-License-Identifier: MIT
#include "rekordbox-memory-fixture.hpp"
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>
#include <iostream>
int wmain(int argc, wchar_t **argv) {
    if (argc != 2) return 2;
    try {
        const std::filesystem::path root(argv[1]); RekordboxFixture fixture(24);
        auto ack = [&](unsigned stage) { const auto tmp = root / "ack.tmp", final = root / "ack.txt"; { std::ofstream out(tmp); out << stage; } if (!MoveFileExW(tmp.c_str(), final.c_str(), MOVEFILE_REPLACE_EXISTING)) throw std::runtime_error("RB ack"); };
        ack(0); { std::ofstream ready(root / "ready.json"); ready << "{\"pid\":" << GetCurrentProcessId() << ",\"module\":" << fixture.base() << "}"; }
        const auto started = std::chrono::steady_clock::now(); unsigned stage = 0; std::array<std::int32_t, 4> positions{};
        while (std::chrono::steady_clock::now() - started < std::chrono::seconds(60)) {
            unsigned command = 0; { std::ifstream input(root / "command.txt"); input >> command; }
            if (command == stage + 1 && command <= 4) {
                if (command == 1) fixture.master(4);
                if (command == 3) fixture.master(2);
                stage = command; ack(stage);
                if (stage == 4) { std::cout << "RB fixture normal Exit; stages=0,1,2,3,4\n"; return 0; }
            }
            for (unsigned i = 0; i < 4; ++i) {
                if (stage < 2 || (stage == 3 && i == 1)) positions[i] += 20;
                fixture.put<std::int32_t>(fixture.positions[i], positions[i]);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        throw std::runtime_error("RB fixture timeout, not PASS");
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 255; }
}
