// SPDX-License-Identifier: MIT
#pragma once

#include "dj-provider.hpp"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <thread>
#include <vector>

namespace playing::providers {

class ProviderRegistry;
class ConsumerToken {
public:
    ConsumerToken() = default;
    explicit operator bool() const noexcept { return sequence_ != 0; }
private:
    friend class ProviderRegistry;
    ConsumerToken(std::uint64_t registry, std::uint64_t sequence)
        : registry_(registry), sequence_(sequence) {}
    std::uint64_t registry_ = 0, sequence_ = 0;
};

using Factory = std::function<std::unique_ptr<IDjProvider>(const Request &)>;
enum class Registration { Registered, Duplicate, Invalid, CapacityExceeded };
enum class Acquisition {
    Acquired, UnknownProvider, InvalidRequest, UnsupportedMode,
    FactoryFailed, StartFailed, CapacityExceeded
};
struct AcquireResult {
    Acquisition result = Acquisition::InvalidRequest;
    ConsumerToken token;
    core::SnapshotPtr snapshot;
};
enum class Refresh {
    Applied, Unchanged, TemporaryFailure, Unavailable,
    InvalidSample, ProviderException, UnknownSession, Pending
};
struct RefreshResult {
    Refresh result = Refresh::UnknownSession;
    core::SnapshotPtr snapshot;
    ProviderIssue issue = ProviderIssue::None;
};

// One host registry, one provider + core per exact Request, many consumers.
// All registry calls and destruction belong to the constructing thread. A
// consumer may pass an obtained immutable snapshot to another thread.
class ProviderRegistry final {
public:
    static constexpr std::size_t maxProviders = 16;
    static constexpr std::size_t maxSessions = 32;
    static constexpr std::size_t maxConsumers = 256;
    ProviderRegistry();
    ~ProviderRegistry();
    ProviderRegistry(const ProviderRegistry &) = delete;
    ProviderRegistry &operator=(const ProviderRegistry &) = delete;
    ProviderRegistry(ProviderRegistry &&) = delete;
    ProviderRegistry &operator=(ProviderRegistry &&) = delete;

    // No providers are registered implicitly. Registration creates no reader.
    Registration registerProvider(Descriptor descriptor, Factory factory);
    std::vector<Descriptor> descriptors() const;
    AcquireResult acquire(const Request &request);
    bool release(ConsumerToken token);
    core::SnapshotPtr snapshot(ConsumerToken token) const;
    // Refresh by session key prevents a per-consumer polling contract. Host
    // scheduling remains outside the registry and provider API.
    RefreshResult refresh(const Request &request);
    core::Update updateArtwork(const Request &request, std::uint64_t trackRevision,
        const core::Artwork &artwork);
    std::vector<Request> activeRequests() const;
    std::size_t consumerCount() const;
    void clear(); // Invalidates all tokens; keeps registered factories.

private:
    struct Entry { Descriptor descriptor; Factory factory; };
    struct Session;
    void requireOwner() const;
    const std::thread::id owner_;
    const std::uint64_t identity_;
    std::uint64_t sequence_ = 0;
    std::map<std::string, Entry> entries_;
    std::map<Request, std::unique_ptr<Session>> sessions_;
    std::map<std::uint64_t, Request> consumers_;
};

} // namespace playing::providers
