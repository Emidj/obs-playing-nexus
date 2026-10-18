// SPDX-License-Identifier: MIT
#include "playing/providers/virtualdj-process.hpp"
#include <windows.h>
#include <tlhelp32.h>
#include <vector>
#include <string>

namespace playing::providers::virtualdj {
namespace {
class Handle {
public:
    explicit Handle(HANDLE value) : value_(value) {}
    ~Handle() { if (value_ && value_ != INVALID_HANDLE_VALUE) CloseHandle(value_); }
    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;
    HANDLE get() const { return value_; }
private: HANDLE value_;
};
class WindowsMemory final : public ProcessMemory {
public:
    explicit WindowsMemory(HANDLE handle) : handle_(handle) {}
    bool alive() const noexcept override {
        DWORD code = 0; return GetExitCodeProcess(handle_.get(), &code) && code == STILL_ACTIVE;
    }
    std::wstring imagePath() const override {
        std::wstring path(32768, L'\0'); DWORD size = static_cast<DWORD>(path.size());
        if (!QueryFullProcessImageNameW(handle_.get(), 0, path.data(), &size)) return {};
        path.resize(size); return path;
    }
    bool read(std::uint64_t address, void *output, std::size_t bytes) override {
        constexpr auto limit = UINT64_C(140737488355328);
        if (!output || address < 65536 || address >= limit || !bytes || bytes > 65536 || bytes >= limit - address) return false;
        SIZE_T actual = 0;
        return ReadProcessMemory(handle_.get(), reinterpret_cast<const void *>(address), output, bytes, &actual) && actual == bytes;
    }
private: Handle handle_;
};
std::string fileVersion(const wchar_t *path) {
    DWORD unused = 0; const auto size = GetFileVersionInfoSizeW(path, &unused);
    if (!size || size > 1048576) return {};
    std::vector<unsigned char> bytes(size);
    if (!GetFileVersionInfoW(path, 0, size, bytes.data())) return {};
    VS_FIXEDFILEINFO *info = nullptr; UINT length = 0;
    if (!VerQueryValueW(bytes.data(), L"\\", reinterpret_cast<void **>(&info), &length)
        || !info || length < sizeof(*info) || info->dwSignature != 0xfeef04bd) return {};
    return std::to_string(HIWORD(info->dwFileVersionMS)) + "." + std::to_string(LOWORD(info->dwFileVersionMS))
        + "." + std::to_string(HIWORD(info->dwFileVersionLS)) + "." + std::to_string(LOWORD(info->dwFileVersionLS));
}
}
std::unique_ptr<ProcessMemory> openReadOnly(std::uint32_t pid) {
    auto handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!handle) return {};
    // Reject WOW64 and non-AMD64 processes without acquiring write rights.
    USHORT process = 0, native = 0;
    if (!IsWow64Process2(handle, &process, &native) || process != IMAGE_FILE_MACHINE_UNKNOWN || native != IMAGE_FILE_MACHINE_AMD64) {
        CloseHandle(handle); return {};
    }
    try { return std::make_unique<WindowsMemory>(handle); }
    catch (...) { CloseHandle(handle); throw; }
}
Connection discoverExecutable(const wchar_t *executable, const std::function<bool(const std::string &)> &acceptVersion, std::uint32_t minimumImageSize) {
    Connection result;
    Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (snapshot.get() == INVALID_HANDLE_VALUE) { result.issue = ProcessIssue::Access; return result; }
    PROCESSENTRY32W entry{}; entry.dwSize = sizeof(entry); DWORD pid = 0;
    if (!Process32FirstW(snapshot.get(), &entry)) { result.issue = ProcessIssue::Access; return result; }
    do {
        if (_wcsicmp(entry.szExeFile, executable) == 0) {
            if (pid) { result.issue = ProcessIssue::Multiple; return result; }
            pid = entry.th32ProcessID;
        }
    } while (Process32NextW(snapshot.get(), &entry));
    if (GetLastError() != ERROR_NO_MORE_FILES) { result.issue = ProcessIssue::Access; return result; }
    if (!pid) return result;
    Handle modules(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid));
    MODULEENTRY32W module{}; module.dwSize = sizeof(module);
    if (modules.get() == INVALID_HANDLE_VALUE || !Module32FirstW(modules.get(), &module)) { result.issue = ProcessIssue::Access; return result; }
    if (_wcsicmp(module.szModule, executable) != 0) { result.issue = ProcessIssue::Architecture; return result; }
    result.version = fileVersion(module.szExePath);
    if (!acceptVersion(result.version)) { result.issue = ProcessIssue::Version; return result; }
    result.memory = openReadOnly(pid);
    if (!result.memory) { result.issue = ProcessIssue::Access; return result; }
    const auto openedPath = result.memory->imagePath();
    if (openedPath.empty() || _wcsicmp(openedPath.c_str(), module.szExePath) != 0
        || fileVersion(openedPath.c_str()) != result.version || !acceptVersion(result.version)) {
        result.memory.reset(); result.issue = ProcessIssue::Version; return result;
    }
    result.module = reinterpret_cast<std::uint64_t>(module.modBaseAddr);
    // Defend against process restart/module enumeration races with a PE header.
    IMAGE_DOS_HEADER dos{}; IMAGE_NT_HEADERS64 nt{};
    if (!result.memory->alive() || !result.memory->read(result.module, &dos, sizeof(dos))
        || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < 0 || dos.e_lfanew > 1048576
        || !result.memory->read(result.module + static_cast<unsigned>(dos.e_lfanew), &nt, sizeof(nt))
        || nt.Signature != IMAGE_NT_SIGNATURE || nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64
        || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC || nt.OptionalHeader.SizeOfImage <= minimumImageSize) {
        result.memory.reset(); result.issue = ProcessIssue::Architecture; return result;
    }
    result.issue = ProcessIssue::None; return result;
}
Connection discover() { return discoverExecutable(L"virtualdj.exe", [](const std::string &version) { return version == verifiedVersion; }, 0x4ea5210); }
}
