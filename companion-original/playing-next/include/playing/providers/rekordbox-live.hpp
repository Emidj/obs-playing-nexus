// SPDX-License-Identifier: MIT
#pragma once
#include "provider-registry.hpp"
#include "virtualdj-process.hpp"
#include <atomic>
namespace playing::providers::rekordbox {
bool acceptsVersion(const std::string &version) noexcept;
// Parsed labels follow the application's language; split only the first ': '.
std::optional<core::Metadata> parseLabels(const std::string &text);
enum class ReadIssue { None, Searching, Unavailable, Version, ChangedSnapshot, AmbiguousMaster };
struct Read { ReadIssue issue = ReadIssue::None; std::optional<unsigned> deck; std::optional<core::Observation> observation; };
class MemoryReader final {
public:
    MemoryReader(virtualdj::Memory &memory, std::uint64_t module, std::string version);
    ~MemoryReader();
    MemoryReader(const MemoryReader &) = delete;
    MemoryReader &operator=(const MemoryReader &) = delete;
    // One discovery slice per call (nominal 4ms CPU, cancellable); then automatic.
    Read read(double now, const std::atomic<bool> &stopping, std::optional<unsigned> manual = {}); // 0=MASTER, 1..4=deck.
private:
    struct Impl; std::unique_ptr<Impl> impl_;
};
using Opener = std::function<virtualdj::Connection()>;
std::unique_ptr<IDjProvider> makeLive(const Request &request, Opener opener = {}, std::function<double()> clock = {},std::string databaseLibrary={});
// Unified History/Automatic registry, trusted DB dependency path fixed by host.
Registration registerProvider(ProviderRegistry &registry, const std::string &absoluteDatabaseLibrary);
}
