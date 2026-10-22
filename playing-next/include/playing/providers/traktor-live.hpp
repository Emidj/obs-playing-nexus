// SPDX-License-Identifier: MIT
#pragma once
#include "provider-registry.hpp"
#include "virtualdj-process.hpp"
#include <array>
#include <atomic>
#include <filesystem>
namespace playing::providers::traktor {
// Windows AMD64 3.x/4.x families. RTTI/layout is validated at runtime; the version
// below records the real local validation, not a gate restricting other 4.x.
inline constexpr const char *verifiedVersion="4.4.1.22";
inline constexpr const char *verifiedVersion3="3.5.0.270";
bool acceptsVersion(const std::string &version) noexcept;
struct Deck {
    std::uint64_t address=0,transport=0;
    bool playing=false;
    std::optional<core::Metadata> track;
    std::string coverKey;
};
struct Frame {std::array<Deck,4> decks;std::uint64_t master=0;};
enum class Issue {None,Searching,Version,Layout,Changed,Ambiguous};
struct Read {Issue issue=Issue::None;Frame frame;};
struct Selection {Issue issue=Issue::None;std::optional<unsigned> deck;core::Observation observation;std::string coverKey;};
Selection select(const Frame &frame,std::optional<unsigned> previous={});
// Owner is the provider worker. Discovery is sliced/cancellable; after discovery
// only cached deck objects are read. No injection or process writes.
class MemoryReader final {
public:
    MemoryReader(virtualdj::ProcessMemory &,std::uint64_t module,std::string version);
    ~MemoryReader();
    MemoryReader(const MemoryReader &)=delete;
    MemoryReader &operator=(const MemoryReader &)=delete;
    Read read(const std::atomic<bool> &stopping);
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
using Opener=std::function<virtualdj::Connection()>;
struct Options {
    Opener opener;
    // Empty uses Windows Documents/Native Instruments/Traktor <version> and
    // the product artwork cache. Explicit roots support isolated QA.
    std::filesystem::path libraryRoot,cacheRoot;
};
std::unique_ptr<IDjProvider> makeLive(const Request &,Options options={});
Registration registerProvider(ProviderRegistry &);
}
