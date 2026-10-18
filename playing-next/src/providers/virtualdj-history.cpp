// SPDX-License-Identifier: MIT
#include "media-artwork.hpp"
#include "playing/providers/virtualdj-history.hpp"
#include <windows.h>
#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <thread>

namespace playing::providers {
namespace {
constexpr std::size_t tailLimit = 65536;
std::wstring wide(const std::string &value)
{
    if (value.empty() || value.size() > 32767 || value.find('\0') != std::string::npos) return {};
    const auto count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), nullptr, 0);
    if (!count) return {};
    std::wstring result(static_cast<std::size_t>(count), L'\0');
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), result.data(), count)) return {};
    return result;
}
std::string utf8(const std::wstring &value)
{
    if (value.empty()) return {};
    const auto count = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
        nullptr, 0, nullptr, nullptr);
    if (!count) throw std::runtime_error("history conversion failed");
    std::string result(static_cast<std::size_t>(count), '\0');
    if (!WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
        result.data(), count, nullptr, nullptr)) throw std::runtime_error("history conversion failed");
    return result;
}
std::wstring fullPath(const std::string &path)
{
    const auto input = wide(path);
    if (input.empty()) return {};
    std::wstring result(32768, L'\0');
    const auto count = GetFullPathNameW(input.c_str(), static_cast<DWORD>(result.size()), result.data(), nullptr);
    if (!count || count >= result.size()) return {};
    result.resize(count); return result;
}
std::vector<std::string> candidates()
{
    std::vector<std::string> result;
    for (const auto *name : {L"LOCALAPPDATA", L"APPDATA", L"USERPROFILE", L"OneDrive", L"OneDriveConsumer"}) {
        std::wstring root(32768, L'\0');
        const auto count = GetEnvironmentVariableW(name, root.data(), static_cast<DWORD>(root.size()));
        if (!count || count >= root.size()) continue;
        root.resize(count);
        root += (std::wstring_view(name) == L"LOCALAPPDATA" || std::wstring_view(name) == L"APPDATA")
            ? L"\\VirtualDJ\\History\\tracklist.txt" : L"\\Documents\\VirtualDJ\\History\\tracklist.txt";
        result.push_back(utf8(root));
    }
    return result;
}
class Handle final {
public:
    explicit Handle(HANDLE value) : value_(value) {}
    ~Handle() { if (value_ != INVALID_HANDLE_VALUE) CloseHandle(value_); }
    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;
    HANDLE get() const noexcept { return value_; }
private:
    HANDLE value_;
};
struct Tail { ProviderIssue issue = ProviderIssue::None; std::string text; };
Tail readTail(const std::string &path, HistoryEncoding encoding)
{
    const auto filename = fullPath(path);
    if (filename.empty()) return {ProviderIssue::ReadError, {}};
    Handle file(CreateFileW(filename.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr));
    if (file.get() == INVALID_HANDLE_VALUE) {
        const auto error = GetLastError();
        return {error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND
            ? ProviderIssue::MissingHistory : ProviderIssue::ReadError, {}};
    }
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file.get(), &size) || size.QuadPart < 0) return {ProviderIssue::ReadError, {}};
    char head[3]{}; DWORD count = 0;
    if (!ReadFile(file.get(), head, 3, &count, nullptr)) return {ProviderIssue::ReadError, {}};
    const std::string_view bom(head, count);
    if (bom.size() >= 2 && static_cast<unsigned char>(bom[0]) == 0xff
        && static_cast<unsigned char>(bom[1]) == 0xfe) encoding = HistoryEncoding::Utf16LE;
    if (bom == std::string_view("\xef\xbb\xbf", 3)) encoding = HistoryEncoding::Automatic;
    LARGE_INTEGER offset{};
    offset.QuadPart = std::max<LONGLONG>(0, size.QuadPart - static_cast<LONGLONG>(tailLimit));
    if (encoding == HistoryEncoding::Utf16LE) offset.QuadPart -= offset.QuadPart % 2;
    if (!SetFilePointerEx(file.get(), offset, nullptr, FILE_BEGIN)) return {ProviderIssue::ReadError, {}};
    std::string bytes(tailLimit, '\0');
    const auto requested = static_cast<DWORD>(std::min<LONGLONG>(tailLimit, size.QuadPart - offset.QuadPart));
    if (!ReadFile(file.get(), bytes.data(), requested, &count, nullptr)) return {ProviderIssue::ReadError, {}};
    bytes.resize(count);
    if (offset.QuadPart > 0 && encoding != HistoryEncoding::Utf16LE) {
        const auto newline = bytes.find('\n');
        if (newline != std::string::npos) bytes.erase(0, newline + 1);
    }
    std::string text;
    if (encoding == HistoryEncoding::Utf16LE) {
        std::wstring decoded(bytes.size() / 2, L'\0');
        for (std::size_t i = 0; i < decoded.size(); ++i)
            decoded[i] = static_cast<wchar_t>(static_cast<unsigned char>(bytes[i * 2])
                | (static_cast<unsigned char>(bytes[i * 2 + 1]) << 8));
        text = utf8(decoded);
    } else {
        if (bytes.compare(0, 3, "\xef\xbb\xbf") == 0) bytes.erase(0, 3);
        const auto validUtf8 = bytes.empty() || MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            bytes.data(), static_cast<int>(bytes.size()), nullptr, 0) != 0;
        if (encoding != HistoryEncoding::Ansi && validUtf8) text = std::move(bytes);
        else if (!bytes.empty()) {
            const auto length = MultiByteToWideChar(CP_ACP, 0, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
            if (!length) return {ProviderIssue::ReadError, {}};
            std::wstring decoded(static_cast<std::size_t>(length), L'\0');
            if (!MultiByteToWideChar(CP_ACP, 0, bytes.data(), static_cast<int>(bytes.size()),
                decoded.data(), length)) return {ProviderIssue::ReadError, {}};
            text = utf8(decoded);
        }
    }
    if (text.compare(0, 3, "\xef\xbb\xbf") == 0) text.erase(0, 3);
    if (offset.QuadPart > 0 && encoding == HistoryEncoding::Utf16LE) {
        const auto newline = text.find('\n');
        if (newline != std::string::npos) text.erase(0, newline + 1);
    }
    return {ProviderIssue::None, std::move(text)};
}
bool space(char c) noexcept { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f'; }
std::string_view trim(std::string_view value)
{
    while (!value.empty() && space(value.front())) value.remove_prefix(1);
    while (!value.empty() && space(value.back())) value.remove_suffix(1);
    return value;
}
bool digit(char c) noexcept { return c >= '0' && c <= '9'; }
struct Track { core::Metadata metadata; std::string raw; };
std::optional<Track> parse(std::string_view line)
{
    if (line.substr(0, 3) == std::string_view("\xef\xbb\xbf", 3)) line.remove_prefix(3);
    line = trim(line);
    if (line.size() < 9 || !digit(line[0]) || !digit(line[1]) || line[2] != ':'
        || !digit(line[3]) || !digit(line[4]) || (line[0] - '0') * 10 + line[1] - '0' > 23
        || (line[3] - '0') * 10 + line[4] - '0' > 59) return {};
    std::size_t i = 5;
    if (line[i] == ':') {
        if (line.size() < 11 || !digit(line[6]) || !digit(line[7])) return {};
        i = 8;
    }
    if (!space(line[i])) return {};
    while (i < line.size() && space(line[i])) ++i;
    if (i == line.size() || (line[i] != ':' && line[i] != '-')) return {};
    if (++i == line.size() || !space(line[i])) return {};
    const auto rest = trim(line.substr(i));
    if (rest.empty()) return {};
    core::Metadata metadata{std::string(rest), {}};
    for (std::size_t split = 1; split + 1 < rest.size(); ++split) {
        if (rest[split] == '-' && space(rest[split - 1]) && space(rest[split + 1])) {
            const auto title = trim(rest.substr(split + 1));
            if (!title.empty()) {
                metadata = {std::string(title), std::string(trim(rest.substr(0, split)))};
                break;
            }
        }
    }
    if (!core::validMetadata(metadata)) return {};
    return Track{std::move(metadata), std::string(line)};
}
std::optional<Track> latest(const std::string &text)
{
    std::optional<Track> track;
    std::size_t position = 0;
    while (position < text.size()) {
        const auto end = text.find('\n', position);
        if (end == std::string::npos) break; // Concurrent trailing append is incomplete.
        auto candidate = parse(std::string_view(text).substr(position, end - position));
        if (candidate) track = std::move(candidate);
        position = end + 1;
    }
    return track;
}
} // namespace

VirtualDjHistory::VirtualDjHistory(Request request, Discovery discovery)
    : request_(std::move(request)), discovery_(std::move(discovery)) {}
struct VirtualDjHistory::Worker {
    std::mutex mutex;
    std::condition_variable wake;
    std::thread thread;
    bool requested = false, busy = false, ready = false, stopping = false;
    Sample completed;
};
VirtualDjHistory::~VirtualDjHistory() { stop(); }
void VirtualDjHistory::start()
{
    if (request_.providerId != "virtualdj" || request_.mode != Mode::History
        || request_.historyPath.size() > 4096
        || (request_.encoding != HistoryEncoding::Automatic && request_.encoding != HistoryEncoding::Ansi
            && request_.encoding != HistoryEncoding::Utf16LE)
        || (!request_.historyPath.empty() && fullPath(request_.historyPath).empty()))
        throw std::invalid_argument("Invalid VirtualDJ history configuration");
    if (worker_) return;
    signature_.clear(); selectedPath_.clear();
    worker_ = std::make_unique<Worker>();
    try {
        worker_->thread = std::thread([this] {
            auto &worker = *worker_;
            std::unique_lock<std::mutex> lock(worker.mutex);
            for (;;) {
                worker.wake.wait(lock, [&] { return worker.stopping || worker.requested; });
                if (worker.stopping) return;
                worker.requested = false; worker.busy = true;
                lock.unlock();
                Sample result;
                try { result = readHistory(); }
                catch (...) { result = {SampleKind::TemporaryFailure, {}, ProviderIssue::ReadError}; }
                lock.lock();
                worker.busy = false;
                if (!worker.stopping) { worker.completed = std::move(result); worker.ready = true; }
            }
        });
    } catch (...) { worker_.reset(); throw; }
}
void VirtualDjHistory::stop() noexcept
{
    if (worker_) {
        {
            std::lock_guard<std::mutex> lock(worker_->mutex);
            worker_->stopping = true;
        }
        worker_->wake.notify_one();
        // Cancel a pending synchronous Windows read on this adapter's worker.
        if (worker_->thread.joinable()) {
            CancelSynchronousIo(worker_->thread.native_handle());
            worker_->thread.join();
        }
        worker_.reset();
    }
    signature_.clear(); selectedPath_.clear();
}
Sample VirtualDjHistory::sample()
{
    if (!worker_) throw std::logic_error("VirtualDJ history reader is stopped");
    auto &worker = *worker_;
    std::lock_guard<std::mutex> lock(worker.mutex);
    if (worker.ready) {
        worker.ready = false;
        return std::move(worker.completed);
    }
    if (!worker.busy && !worker.requested) {
        worker.requested = true; worker.wake.notify_one();
    }
    return {SampleKind::Pending, {}}; // No wait and no stale publication.
}
Sample VirtualDjHistory::readHistory()
{
    Tail tail;
    std::string path = request_.historyPath.empty() ? selectedPath_ : request_.historyPath;
    if (!path.empty()) tail = readTail(path, request_.encoding);
    if (path.empty() || (request_.historyPath.empty() && tail.issue == ProviderIssue::MissingHistory)) {
        path.clear(); selectedPath_.clear();
        const auto paths = discovery_ ? discovery_() : candidates();
        // Actual discovery has five candidates. Custom discovery is bounded too.
        if (paths.size() > 16) return {SampleKind::TemporaryFailure, {}, ProviderIssue::ReadError};
        std::vector<std::wstring> seen;
        ProviderIssue failure = ProviderIssue::MissingHistory;
        for (const auto &candidate : paths) {
            const auto canonical = fullPath(candidate);
            if (canonical.empty()) { failure = ProviderIssue::ReadError; continue; }
            const bool duplicate = std::any_of(seen.begin(), seen.end(), [&](const auto &previous) {
                return CompareStringOrdinal(previous.c_str(), static_cast<int>(previous.size()),
                    canonical.c_str(), static_cast<int>(canonical.size()), TRUE) == CSTR_EQUAL;
            });
            if (duplicate) continue;
            seen.push_back(canonical);
            auto found = readTail(candidate, request_.encoding);
            if (found.issue != ProviderIssue::None) {
                if (found.issue != ProviderIssue::MissingHistory) failure = found.issue;
                continue;
            }
            if (!path.empty()) return {SampleKind::TemporaryFailure, {}, ProviderIssue::AmbiguousHistory};
            path = candidate; tail = std::move(found);
        }
        if (path.empty()) tail = {failure, {}};
        else selectedPath_ = path;
    }
    if (tail.issue == ProviderIssue::MissingHistory) {
        signature_.clear();
        return {SampleKind::Unavailable, {}, tail.issue};
    }
    if (tail.issue != ProviderIssue::None) return {SampleKind::TemporaryFailure, {}, tail.issue};
    auto track = latest(tail.text);
    if (!track) {
        if (!signature_.empty()) return {SampleKind::NoChange, {}, ProviderIssue::NoRecords};
        return {SampleKind::Observation, core::Observation{core::Playback::Waiting, {}}, ProviderIssue::NoRecords};
    }
    if (signature_ == path + '\0' + track->raw) return {};
    signature_ = path + '\0' + track->raw;
    detail::VirtualDjCover cover;cover.enrich(track->metadata);
    return {SampleKind::Observation, core::Observation{core::Playback::Playing, std::move(track->metadata)}};
}
Registration registerVirtualDjHistory(ProviderRegistry &registry)
{
    return registry.registerProvider({"virtualdj", "VirtualDJ", false, true},
        [](const Request &request) { return std::make_unique<VirtualDjHistory>(request); });
}
} // namespace playing::providers
