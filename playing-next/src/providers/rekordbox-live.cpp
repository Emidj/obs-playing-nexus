// SPDX-License-Identifier: MIT
#include "playing/providers/rekordbox-live.hpp"
#include "playing/providers/rekordbox-history.hpp"
#include "worker-provider.hpp"
#include "library-artwork.hpp"
#include <chrono>
#include <windows.h>
#include <filesystem>
namespace playing::providers::rekordbox {
namespace {
Sample failed(virtualdj::ProcessIssue issue) {
    using virtualdj::ProcessIssue;
    switch (issue) {
    case ProcessIssue::Missing: return {SampleKind::Unavailable, {}, ProviderIssue::ProcessMissing};
    case ProcessIssue::Multiple: return {SampleKind::Unavailable, {}, ProviderIssue::ProcessMultiple};
    case ProcessIssue::Version: return {SampleKind::Unavailable, {}, ProviderIssue::UnsupportedVersion};
    case ProcessIssue::Architecture: return {SampleKind::Unavailable, {}, ProviderIssue::InvalidLayout};
    default: return {SampleKind::TemporaryFailure, {}, ProviderIssue::ProcessAccess};
    }
}
class Reader final : public detail::WorkerReader {
public:
    Reader(Opener opener, std::function<double()> clock,std::string library) : opener_(std::move(opener)), clock_(std::move(clock)),cover_(std::move(library)) {}
    Sample read(const std::atomic<bool> &stopping) override {
        if (stopping) return {SampleKind::Pending, {}};
        if (!connection_.memory || !connection_.memory->alive()) {
            close(); connection_ = opener_ ? opener_() : virtualdj::discoverExecutable(L"rekordbox.exe", acceptsVersion);
            if (connection_.issue != virtualdj::ProcessIssue::None || !connection_.memory) return failed(connection_.issue);
            reader_ = std::make_unique<MemoryReader>(*connection_.memory, connection_.module, connection_.version);
        }
        const auto now = clock_ ? clock_() : std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        auto result = reader_->read(now, stopping);
        if(result.issue==ReadIssue::None&&result.observation&&result.observation->metadata)cover_.enrich(*result.observation->metadata);
        if (!connection_.memory->alive()) { close(); return failed(virtualdj::ProcessIssue::Missing); }
        switch (result.issue) {
        case ReadIssue::None: return {SampleKind::Observation, result.observation};
        case ReadIssue::Searching: return {SampleKind::Pending, {}};
        case ReadIssue::ChangedSnapshot: return {SampleKind::TemporaryFailure, {}, ProviderIssue::SnapshotChanged};
        case ReadIssue::AmbiguousMaster: return {SampleKind::TemporaryFailure, {}, ProviderIssue::AmbiguousSelection};
        case ReadIssue::Version: close(); return {SampleKind::Unavailable, {}, ProviderIssue::UnsupportedVersion};
        default: close(); return {SampleKind::Unavailable, {}, ProviderIssue::InvalidLayout};
        }
    }
    void close() noexcept override { reader_.reset(); connection_ = {}; }
private: Opener opener_; std::function<double()> clock_; virtualdj::Connection connection_; std::unique_ptr<MemoryReader> reader_;detail::RekordboxCover cover_;
};
}
std::unique_ptr<IDjProvider> makeLive(const Request &request, Opener opener, std::function<double()> clock,std::string databaseLibrary) {
    if (request.providerId != "rekordbox" || request.mode != Mode::Automatic || !request.historyPath.empty()
        || request.encoding != HistoryEncoding::Automatic) throw std::invalid_argument("Invalid rekordbox automatic request");
    return std::make_unique<detail::WorkerProvider>([opener, clock,databaseLibrary] { return std::make_unique<Reader>(opener, clock,databaseLibrary); });
}
Registration registerProvider(ProviderRegistry &registry, const std::string &library) {
    if (library.empty() || library.size() > 4096 || library.find('\0') != std::string::npos
        || !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, library.data(), static_cast<int>(library.size()), nullptr, 0)
        || !std::filesystem::u8path(library).is_absolute()) return Registration::Invalid;
    return registry.registerProvider({"rekordbox", "rekordbox", true, true}, [library](const Request &request) -> std::unique_ptr<IDjProvider> {
        if (request.mode == Mode::History) return std::make_unique<RekordboxHistory>(request, library);
        return makeLive(request,{}, {},library);
    });
}
}
