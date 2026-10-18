// SPDX-License-Identifier: MIT
#include "playing/providers/rekordbox-history.hpp"
#include "rekordbox-database.hpp"
#include "rekordbox-options.hpp"
#include "library-artwork.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <algorithm>

namespace playing::providers {
namespace rb = rekordbox;
namespace {
struct FileHandle {
    HANDLE value;
    ~FileHandle() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};
struct Identity {
    DWORD volume = 0, high = 0, low = 0;
    bool operator==(const Identity &other) const noexcept
    { return volume == other.volume && high == other.high && low == other.low; }
};
ProviderIssue identity(const std::string &path, Identity &result)
{
    const auto name = std::filesystem::u8path(path);
    const FileHandle file{CreateFileW(name.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE |
        FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
    if (file.value == INVALID_HANDLE_VALUE) {
        const auto error = GetLastError();
        return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND
            ? ProviderIssue::MissingHistory : ProviderIssue::ReadError;
    }
    BY_HANDLE_FILE_INFORMATION info{};
    const auto ok = GetFileInformationByHandle(file.value, &info);
    if (!ok || (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) return ProviderIssue::ReadError;
    result = {info.dwVolumeSerialNumber, info.nFileIndexHigh, info.nFileIndexLow};
    return ProviderIssue::None;
}
bool pathValid(const std::string &path)
{
    return !path.empty() && path.size() <= 4096 && path.find('\0') == std::string::npos
        && MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.data(),
            static_cast<int>(path.size()), nullptr, 0) != 0
        && std::filesystem::u8path(path).is_absolute();
}
RekordboxHistoryLocations locations()
{
    std::wstring appdata(32768, L'\0');
    const auto count = GetEnvironmentVariableW(L"APPDATA", appdata.data(), static_cast<DWORD>(appdata.size()));
    if (!count || count >= appdata.size()) return {};
    appdata.resize(count);
    RekordboxHistoryLocations result;
    for (const auto *brand : {L"Pioneer", L"AlphaTheta"}) {
        const auto base = std::filesystem::path(appdata) / brand;
        result.optionsFiles.push_back((base / L"rekordboxAgent/storage/options.json").u8string());
        result.databases.push_back((base / L"rekordbox/master.db").u8string());
    }
    return result;
}
ProviderIssue optionPaths(const std::string &path, std::vector<std::string> &paths)
{
    if (!pathValid(path)) return ProviderIssue::ReadError;
    const auto name = std::filesystem::u8path(path);
    const FileHandle file{CreateFileW(name.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE |
        FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
    if (file.value == INVALID_HANDLE_VALUE) {
        const auto error = GetLastError();
        return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND
            ? ProviderIssue::MissingHistory : ProviderIssue::ReadError;
    }
    // An options file is bounded before allocation; shared partial writes are rejected.
    LARGE_INTEGER length{};
    if (!GetFileSizeEx(file.value, &length) || length.QuadPart < 0 || length.QuadPart > 65536)
        return ProviderIssue::ReadError;
    std::string bytes(static_cast<std::size_t>(length.QuadPart), '\0');
    DWORD read = 0;
    const auto ok = ReadFile(file.value, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr);
    if (!ok || read != bytes.size() || bytes.empty()) return ProviderIssue::ReadError;
    if (bytes.compare(0, 3, "\xef\xbb\xbf") == 0) bytes.erase(0, 3);
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(),
        static_cast<int>(bytes.size()), nullptr, 0)) return ProviderIssue::ReadError;
    try {
        auto found = rb::OptionsJson(bytes).paths();
        if (paths.size() + found.size() > 16) return ProviderIssue::ReadError;
        for (auto &candidate : found) {
            if (!pathValid(candidate)) return ProviderIssue::ReadError;
            paths.push_back(std::move(candidate));
        }
    } catch (...) { return ProviderIssue::ReadError; }
    return ProviderIssue::None;
}
ProviderIssue discover(const RekordboxHistoryLocations &source, std::string &selected)
{
    if (source.optionsFiles.size() > 16 || source.databases.size() > 16) return ProviderIssue::ReadError;
    std::vector<std::string> preferred;
    for (const auto &options : source.optionsFiles) {
        const auto issue = optionPaths(options, preferred);
        if (issue != ProviderIssue::None && issue != ProviderIssue::MissingHistory) return issue;
    }
    // Like the real Lua, relocated entries take precedence over standard libraries.
    // Do not silently show a stale standard library when a declared relocation is absent.
    const auto &paths = preferred.empty() ? source.databases : preferred;
    std::vector<Identity> seen;
    std::string chosen;
    ProviderIssue failed = ProviderIssue::None;
    for (const auto &path : paths) {
        if (!pathValid(path)) return ProviderIssue::ReadError;
        Identity id;
        const auto issue = identity(path, id);
        if (issue == ProviderIssue::MissingHistory) continue;
        if (issue != ProviderIssue::None) { failed = issue; continue; }
        if (std::find(seen.begin(), seen.end(), id) != seen.end()) continue;
        seen.push_back(id);
        if (seen.size() > 1) return ProviderIssue::AmbiguousHistory;
        chosen = std::filesystem::u8path(path).lexically_normal().u8string();
    }
    if (failed != ProviderIssue::None) { selected.clear(); return failed; }
    if (chosen.empty()) return ProviderIssue::MissingHistory;
    selected = std::move(chosen);
    return ProviderIssue::None;
}
Sample failure(ProviderIssue issue)
{
    const bool unavailable = issue == ProviderIssue::MissingHistory || issue == ProviderIssue::DatabaseFormat
        || issue == ProviderIssue::DatabaseSchema;
    return {unavailable ? SampleKind::Unavailable : SampleKind::TemporaryFailure, {}, issue};
}
}
struct RekordboxHistory::Worker {
    std::mutex mutex;
    std::condition_variable wake;
    std::thread thread;
    std::atomic<bool> stopping{false};
    bool requested = false, busy = false, ready = false;
    Sample completed;
    // Only this worker touches the library, connection and cached data.
    rb::Api api;
    rb::Connection connection{api};
    Identity currentIdentity;
    std::string version, signature, selectedPath;
    std::chrono::steady_clock::time_point deadline;
    static int interrupt(void *context) noexcept
    {
        const auto &self = *static_cast<Worker *>(context);
        return self.stopping.load() || std::chrono::steady_clock::now() >= self.deadline;
    }
    Sample databaseFailure(int code)
    {
        ProviderIssue issue = ProviderIssue::ReadError;
        switch (code & 255) {
        case 5: case 6: issue = ProviderIssue::DatabaseBusy; break;
        case 11: case 26: issue = ProviderIssue::DatabaseFormat; break;
        case 1: case 17: issue = ProviderIssue::DatabaseSchema; break;
        case 9: issue = ProviderIssue::ReadTimeout; break;
        case 18: issue = ProviderIssue::InvalidMetadata; break;
        }
        connection.reset(); version.clear(); signature.clear();
        return failure(issue);
    }
    Sample readDatabase(const Request &request, const std::string &libraryPath)
    {
        Identity found;
        auto issue = identity(request.historyPath, found);
        if (issue != ProviderIssue::None) {
            connection.reset(); version.clear(); signature.clear();
            return failure(issue);
        }
        try { api.load(libraryPath); }
        catch (...) { return failure(ProviderIssue::DependencyUnavailable); }
        deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(50);
        if (!connection.db || !(currentIdentity == found)) {
            connection.reset(); version.clear(); signature.clear();
            const int code = api.open(request.historyPath.c_str(), &connection.db, 0x10001, nullptr);
            if (code != 0) return databaseFailure(code);
            api.progress(connection.db, 1000, interrupt, this);
            // Cap pathological row/SQL allocations; metadata still has the stricter Core bound.
            api.limit(connection.db, 0, 1024 * 1024); // SQLITE_LIMIT_LENGTH
            if (api.readonly(connection.db, "main") != 1 || api.timeout(connection.db, 25) != 0) {
                connection.reset(); return failure(ProviderIssue::ReadError);
            }
            int setup = api.exec(connection.db, rb::keySql, nullptr, nullptr, nullptr);
            if (setup == 0) setup = api.exec(connection.db,
                "PRAGMA query_only=ON; PRAGMA trusted_schema=OFF;", nullptr, nullptr, nullptr);
            if (setup != 0) return databaseFailure(setup);
            Identity afterOpen;
            if (identity(request.historyPath, afterOpen) != ProviderIssue::None || !(found == afterOpen)) {
                connection.reset(); return failure(ProviderIssue::ReadError);
            }
            currentIdentity = found;
        }
        rb::Statement dataVersion(api);
        int code = api.prepare(connection.db, "PRAGMA data_version", -1, &dataVersion.value, nullptr);
        if (code == 0) code = api.step(dataVersion.value);
        if (code != 100) return databaseFailure(code);
        const auto rawVersion = api.text(dataVersion.value, 0);
        const std::string nextVersion = rawVersion ? reinterpret_cast<const char *>(rawVersion) : "";
        if (!version.empty() && nextVersion == version) return {};
        rb::Statement statement(api);
        code = api.prepare(connection.db, rb::latestSql, -1, &statement.value, nullptr);
        if (code == 0) code = api.step(statement.value);
        if (code == 101) {
            version = nextVersion; signature.clear();
            return {SampleKind::Observation, core::Observation{core::Playback::Waiting, {}}, ProviderIssue::NoRecords};
        }
        if (code != 100) return databaseFailure(code);
        std::string fields[6];
        for (int column = 0; column < 6; ++column) {
            const auto text = api.text(statement.value, column);
            const auto count = api.bytes(statement.value, column);
            if (count < 0 || count > 4096) return failure(ProviderIssue::InvalidMetadata);
            if (text) fields[column].assign(reinterpret_cast<const char *>(text), static_cast<std::size_t>(count));
        }
        core::Metadata metadata{fields[3], fields[4]};
        if (!core::validMetadata(metadata)) return failure(ProviderIssue::InvalidMetadata);
        detail::RekordboxCover cover(libraryPath);cover.enrich(metadata,request.historyPath,fields[5]);
        std::string nextSignature;
        for (const auto &field : fields) { nextSignature += std::to_string(field.size()) + ':' + field; }
        if(metadata.artwork.image)nextSignature+=metadata.artwork.image->location+metadata.artwork.image->version;
        version = nextVersion;
        if (signature == nextSignature) return {};
        signature = std::move(nextSignature);
        return {SampleKind::Observation, core::Observation{core::Playback::Playing, std::move(metadata)}};
    }
    Sample read(const Request &request, const std::string &libraryPath, const Discovery &discovery)
    {
        Request actual = request;
        if (actual.historyPath.empty()) {
            Identity ignored;
            if (selectedPath.empty() || identity(selectedPath, ignored) == ProviderIssue::MissingHistory) {
                connection.reset(); version.clear(); signature.clear(); selectedPath.clear();
                const auto issue = discover(discovery ? discovery() : locations(), selectedPath);
                if (issue != ProviderIssue::None) return failure(issue);
            }
            actual.historyPath = selectedPath;
        }
        auto result = readDatabase(actual, libraryPath);
        if (connection.db) {
            Identity afterRead;
            const auto issue = identity(actual.historyPath, afterRead);
            if (issue != ProviderIssue::None || !(afterRead == currentIdentity)) {
                connection.reset(); version.clear(); signature.clear();
                return failure(issue == ProviderIssue::None ? ProviderIssue::ReadError : issue);
            }
        }
        return result;
    }
};
RekordboxHistory::RekordboxHistory(Request request, std::string libraryPath, Discovery discovery)
    : request_(std::move(request)), libraryPath_(std::move(libraryPath)), discovery_(std::move(discovery)) {}
RekordboxHistory::~RekordboxHistory() { stop(); }
void RekordboxHistory::start()
{
    if (request_.providerId != "rekordbox" || request_.mode != Mode::History
        || request_.encoding != HistoryEncoding::Automatic
        || (!request_.historyPath.empty() && !pathValid(request_.historyPath))
        || !pathValid(libraryPath_)) throw std::invalid_argument("Invalid rekordbox history configuration");
    if (worker_) return;
    worker_ = std::make_unique<Worker>();
    try {
        worker_->thread = std::thread([this] {
            auto &worker = *worker_;
            std::unique_lock<std::mutex> lock(worker.mutex);
            while (!worker.stopping.load()) {
                worker.wake.wait(lock, [&] { return worker.stopping.load() || worker.requested; });
                if (worker.stopping.load()) break;
                worker.requested = false; worker.busy = true;
                lock.unlock();
                Sample result;
                try { result = worker.read(request_, libraryPath_, discovery_); }
                catch (...) { result = failure(ProviderIssue::ReadError); }
                lock.lock();
                worker.busy = false;
                if (!worker.stopping.load()) { worker.completed = std::move(result); worker.ready = true; }
            }
            lock.unlock();
            worker.connection.reset(); // Library/connection lifetime stays on worker.
            worker.api.unload();
        });
    } catch (...) { worker_.reset(); throw; }
}
void RekordboxHistory::stop() noexcept
{
    if (!worker_) return;
    {
        std::lock_guard<std::mutex> lock(worker_->mutex);
        worker_->stopping = true;
    }
    worker_->wake.notify_one();
    if (worker_->thread.joinable()) {
        CancelSynchronousIo(worker_->thread.native_handle());
        worker_->thread.join();
    }
    worker_.reset();
}
Sample RekordboxHistory::sample()
{
    if (!worker_) throw std::logic_error("rekordbox history reader stopped");
    auto &worker = *worker_;
    std::lock_guard<std::mutex> lock(worker.mutex);
    if (worker.ready) { worker.ready = false; return std::move(worker.completed); }
    if (!worker.busy && !worker.requested) { worker.requested = true; worker.wake.notify_one(); }
    return {SampleKind::Pending, {}};
}
Registration registerRekordboxHistory(ProviderRegistry &registry, std::string libraryPath,
    RekordboxHistory::Discovery discovery)
{
    if (!pathValid(libraryPath)) return Registration::Invalid;
    return registry.registerProvider({"rekordbox", "rekordbox", false, true},
        [libraryPath = std::move(libraryPath), discovery = std::move(discovery)](const Request &request) {
            return std::make_unique<RekordboxHistory>(request, libraryPath, discovery);
        });
}
std::optional<std::string> discoverRekordboxDatabase(){std::string path;return discover(locations(),path)==ProviderIssue::None?std::optional<std::string>{path}:std::nullopt;}
} // namespace playing::providers
