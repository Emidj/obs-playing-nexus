// SPDX-License-Identifier: MIT
#include "playing/providers/provider-registry.hpp"

#include <atomic>
#include <stdexcept>
#include <utility>

namespace playing::providers {
namespace {
std::atomic<std::uint64_t> identities{0};
bool validId(const std::string &id)
{
    if (id.empty() || id.size() > 64) return false;
    for (const unsigned char c : id)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
            || c == '-' || c == '_' || c == '.')) return false;
    return true;
}
bool validRequest(const Request &request)
{
    if (request.encoding != HistoryEncoding::Automatic && request.encoding != HistoryEncoding::Ansi
        && request.encoding != HistoryEncoding::Utf16LE) return false;
    return validId(request.providerId) && request.historyPath.size() <= 4096
        && request.historyPath.find('\0') == std::string::npos
        && ((request.mode == Mode::Automatic && request.historyPath.empty()
            && request.encoding == HistoryEncoding::Automatic)
            || request.mode == Mode::History);
}
} // namespace

struct ProviderRegistry::Session {
    explicit Session(std::unique_ptr<IDjProvider> value) : provider(std::move(value)) {}
    ~Session() { provider->stop(); }
    std::unique_ptr<IDjProvider> provider;
    core::PlayingCore core;
    std::size_t consumers = 0;
};

ProviderRegistry::ProviderRegistry()
    : owner_(std::this_thread::get_id()), identity_(++identities)
{
}
ProviderRegistry::~ProviderRegistry() = default;

void ProviderRegistry::requireOwner() const
{
    if (std::this_thread::get_id() != owner_)
        throw std::logic_error("ProviderRegistry requires the owner thread");
}

Registration ProviderRegistry::registerProvider(Descriptor descriptor, Factory factory)
{
    requireOwner();
    if (!validId(descriptor.id) || descriptor.name.empty() || descriptor.name.size() > 128
        || descriptor.name.find('\0') != std::string::npos
        || (!descriptor.automatic && !descriptor.history) || !factory)
        return Registration::Invalid;
    if (entries_.find(descriptor.id) != entries_.end()) return Registration::Duplicate;
    if (entries_.size() >= maxProviders) return Registration::CapacityExceeded;
    const auto id = descriptor.id;
    entries_.emplace(id, Entry{std::move(descriptor), std::move(factory)});
    return Registration::Registered;
}

std::vector<Descriptor> ProviderRegistry::descriptors() const
{
    requireOwner();
    std::vector<Descriptor> result;
    for (const auto &entry : entries_) result.push_back(entry.second.descriptor);
    return result;
}

AcquireResult ProviderRegistry::acquire(const Request &request)
{
    requireOwner();
    if (!validRequest(request)) return {Acquisition::InvalidRequest, {}, {}};
    const auto entry = entries_.find(request.providerId);
    if (entry == entries_.end()) return {Acquisition::UnknownProvider, {}, {}};
    if ((request.mode == Mode::Automatic && !entry->second.descriptor.automatic)
        || (request.mode == Mode::History && !entry->second.descriptor.history))
        return {Acquisition::UnsupportedMode, {}, {}};
    if (consumers_.size() >= maxConsumers) return {Acquisition::CapacityExceeded, {}, {}};

    auto session = sessions_.find(request);
    const bool created = session == sessions_.end();
    if (created) {
        if (sessions_.size() >= maxSessions) return {Acquisition::CapacityExceeded, {}, {}};
        std::unique_ptr<IDjProvider> provider;
        try { provider = entry->second.factory(request); }
        catch (...) { return {Acquisition::FactoryFailed, {}, {}}; }
        if (!provider) return {Acquisition::FactoryFailed, {}, {}};
        auto fresh = std::make_unique<Session>(std::move(provider));
        try {
            fresh->core.start();
            fresh->provider->start();
        } catch (...) {
            // Session destruction stops even a partly initialized reader.
            return {Acquisition::StartFailed, {}, {}};
        }
        session = sessions_.emplace(request, std::move(fresh)).first;
    }

    const auto next = sequence_ + 1;
    try { consumers_.emplace(next, request); }
    catch (...) {
        if (created) sessions_.erase(session);
        throw;
    }
    sequence_ = next;
    ++session->second->consumers;
    return {Acquisition::Acquired, ConsumerToken{identity_, next}, session->second->core.snapshot()};
}

bool ProviderRegistry::release(ConsumerToken token)
{
    requireOwner();
    if (token.registry_ != identity_) return false;
    const auto consumer = consumers_.find(token.sequence_);
    if (consumer == consumers_.end()) return false;
    const auto session = sessions_.find(consumer->second);
    consumers_.erase(consumer);
    if (--session->second->consumers == 0) sessions_.erase(session);
    return true;
}

core::SnapshotPtr ProviderRegistry::snapshot(ConsumerToken token) const
{
    requireOwner();
    if (token.registry_ != identity_) return {};
    const auto consumer = consumers_.find(token.sequence_);
    if (consumer == consumers_.end()) return {};
    return sessions_.at(consumer->second)->core.snapshot();
}

RefreshResult ProviderRegistry::refresh(const Request &request)
{
    requireOwner();
    const auto session = sessions_.find(request);
    if (session == sessions_.end()) return {};
    auto &core = session->second->core;
    Sample sample;
    try { sample = session->second->provider->sample(); }
    catch (...) { return {Refresh::ProviderException, core.snapshot()}; }
    if (sample.issue != ProviderIssue::None && sample.issue != ProviderIssue::MissingHistory
        && sample.issue != ProviderIssue::AmbiguousHistory && sample.issue != ProviderIssue::ReadError
        && sample.issue != ProviderIssue::NoRecords && sample.issue != ProviderIssue::DatabaseBusy
        && sample.issue != ProviderIssue::DatabaseFormat && sample.issue != ProviderIssue::DatabaseSchema
        && sample.issue != ProviderIssue::DependencyUnavailable && sample.issue != ProviderIssue::InvalidMetadata
        && sample.issue != ProviderIssue::ReadTimeout && sample.issue != ProviderIssue::ProcessMissing
        && sample.issue != ProviderIssue::ProcessMultiple && sample.issue != ProviderIssue::ProcessAccess
        && sample.issue != ProviderIssue::UnsupportedVersion && sample.issue != ProviderIssue::InvalidLayout
        && sample.issue != ProviderIssue::SnapshotChanged && sample.issue != ProviderIssue::ConnectionLost
        && sample.issue != ProviderIssue::AmbiguousSelection) return {Refresh::InvalidSample, core.snapshot()};
    if (sample.kind == SampleKind::Observation) {
        if (!sample.observation) return {Refresh::InvalidSample, core.snapshot()};
        const auto prepare = [&](std::optional<core::TrackState> &value) {
            if (!value) return true;
            auto &track = *value;
            if (track.origin && track.origin->provider != request.providerId
                && !entries_.at(request.providerId).descriptor.remote)
                return false;
            if (!track.origin) track.origin = core::TrackOrigin{request.providerId,
                request.mode == Mode::Automatic ? "automatic" : "history"};
            return true;
        };
        if (!prepare(sample.observation->metadata) || !prepare(sample.observation->retainedTrack))
            return {Refresh::InvalidSample, core.snapshot()};
        const auto update = core.observe(*sample.observation);
        if (update == core::Update::InvalidObservation)
            return {Refresh::InvalidSample, core.snapshot()};
        return {update == core::Update::Applied ? Refresh::Applied : Refresh::Unchanged,
            core.snapshot(), sample.issue};
    }
    if (sample.observation) return {Refresh::InvalidSample, core.snapshot()};
    switch (sample.kind) {
    case SampleKind::Pending:
        return {sample.issue == ProviderIssue::None ? Refresh::Pending : Refresh::InvalidSample, core.snapshot()};
    case SampleKind::NoChange: return {Refresh::Unchanged, core.snapshot(), sample.issue};
    case SampleKind::TemporaryFailure: return {Refresh::TemporaryFailure, core.snapshot(), sample.issue};
    case SampleKind::Unavailable:
        core.observe({core::Playback::Unavailable, std::nullopt});
        return {Refresh::Unavailable, core.snapshot(), sample.issue};
    default: return {Refresh::InvalidSample, core.snapshot()};
    }
}

core::Update ProviderRegistry::updateArtwork(const Request &request, std::uint64_t token,
    const core::Artwork &artwork)
{
    requireOwner();
    const auto session = sessions_.find(request);
    if (session == sessions_.end()) return core::Update::StaleTrack;
    return session->second->core.updateArtwork(token, artwork);
}

std::vector<Request> ProviderRegistry::activeRequests() const
{
    requireOwner();
    std::vector<Request> result;
    for (const auto &session : sessions_) result.push_back(session.first);
    return result;
}

std::size_t ProviderRegistry::consumerCount() const
{
    requireOwner(); return consumers_.size();
}

void ProviderRegistry::clear()
{
    requireOwner(); consumers_.clear(); sessions_.clear();
}

} // namespace playing::providers
