// SPDX-License-Identifier: MIT
#include "playing/providers/virtualdj-process.hpp"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <chrono>
using namespace playing::providers::virtualdj;
int wmain(int argc, wchar_t **argv) {
    if (argc == 4 && std::wstring(argv[1]) == L"--fixture") {
        const auto event = OpenEventW(SYNCHRONIZE, FALSE, argv[3]);
        if (!event) return 2;
        const std::uint64_t value = UINT64_C(0x1122334455667788);
        { std::ofstream out{std::filesystem::path(argv[2])}; out << reinterpret_cast<std::uint64_t>(&value); }
        const auto status = WaitForSingleObject(event, 15000); CloseHandle(event);
        return status == WAIT_OBJECT_0 ? 0 : 3;
    }
    const auto id = std::to_wstring(GetCurrentProcessId());
    const auto eventName = L"Local\\PlayingReadOnlyFixture" + id;
    const auto event = CreateEventW(nullptr, TRUE, FALSE, eventName.c_str());
    wchar_t exe[32768]{}; GetModuleFileNameW(nullptr, exe, 32768);
    const auto ready = std::filesystem::temp_directory_path() / (L"playing-readonly-" + id + L".txt");
    std::filesystem::remove(ready);
    std::wstring command = L"\"" + std::wstring(exe) + L"\" --fixture \"" + ready.wstring() + L"\" " + eventName;
    STARTUPINFOW startup{}; startup.cb = sizeof(startup); PROCESS_INFORMATION child{};
    if (!event || !CreateProcessW(exe, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &child)) return 4;
    int result = 0; unsigned checks = 0;
    auto check = [&result, &checks](bool ok) { ++checks; if (!ok) { std::cerr << "failed " << checks << '\n'; result = 1; } };
    std::uint64_t address = 0;
    for (unsigned i = 0; i < 500 && !address; ++i) {
        std::ifstream input{ready}; input >> address;
        if (!address) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    auto memory = openReadOnly(child.dwProcessId); check(memory && memory->alive()); check(address >= 65536);
    bool versionCalled = false;
    const auto fixtureName = std::filesystem::path(exe).filename().wstring();
    const auto multiple = discoverExecutable(fixtureName.c_str(), [&](const std::string &) { versionCalled = true; return false; });
    check(multiple.issue == ProcessIssue::Multiple && !multiple.memory && !versionCalled);
    if (memory && address) {
        std::uint64_t value = 0;
        check(memory->read(address, &value, sizeof(value)) && value == UINT64_C(0x1122334455667788));
        check(!memory->read(0, &value, sizeof(value))); check(!memory->read(UINT64_MAX, &value, sizeof(value)));
        check(!memory->read(address, nullptr, sizeof(value))); check(!memory->read(address, &value, 65537));
        check(!memory->read(address, &value, 0)); check(!memory->read(65536, &value, sizeof(value)));
        SetEvent(event); check(WaitForSingleObject(child.hProcess, 5000) == WAIT_OBJECT_0);
        DWORD code = 1; GetExitCodeProcess(child.hProcess, &code); check(code == 0); check(!memory->alive());
        check(!memory->read(address, &value, sizeof(value)));
    } else SetEvent(event);
    if (WaitForSingleObject(child.hProcess, 5000) != WAIT_OBJECT_0) { TerminateProcess(child.hProcess, 5); result = 1; }
    CloseHandle(child.hThread); CloseHandle(child.hProcess); CloseHandle(event);
    memory.reset(); check(!openReadOnly(0)); std::filesystem::remove(ready);
    const auto unverified = discoverExecutable(fixtureName.c_str(), [&](const std::string &) { versionCalled = true; return false; });
    check(unverified.issue == ProcessIssue::Version && !unverified.memory && versionCalled);
    const auto missing = discoverExecutable(L"playing-phase05-missing-fixture.exe", [](const std::string &) { return true; });
    check(missing.issue == ProcessIssue::Missing && !missing.memory);
    std::cout << checks << " read-only Windows checks; own fixture process, no DJ compatibility claim\n";
    return result;
}
