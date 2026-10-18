// SPDX-License-Identifier: MIT
#include "playing/core/playing-core.hpp"

#include <atomic>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace playing::core {
namespace {
std::atomic<std::uint64_t> trackTokens{0}; // Process-local async correlation, not a DJ identity.
bool validText(std::string_view text) noexcept
{
    if (text.size() > PlayingCore::maxMetadataBytes)
        return false;
    for (std::size_t i = 0; i < text.size();) {
        const auto first = static_cast<unsigned char>(text[i++]);
        if (first < 0x80) {
            if (first < 0x20 || first == 0x7f)
                return false;
            continue;
        }
        unsigned remaining = 0;
        std::uint32_t value = 0, minimum = 0;
        if (first >= 0xc2 && first <= 0xdf) {
            remaining = 1; value = first & 0x1f; minimum = 0x80;
        } else if (first >= 0xe0 && first <= 0xef) {
            remaining = 2; value = first & 0x0f; minimum = 0x800;
        } else if (first >= 0xf0 && first <= 0xf4) {
            remaining = 3; value = first & 0x07; minimum = 0x10000;
        } else {
            return false;
        }
        if (remaining > text.size() - i)
            return false;
        while (remaining--) {
            const auto next = static_cast<unsigned char>(text[i++]);
            if ((next & 0xc0) != 0x80)
                return false;
            value = (value << 6) | (next & 0x3f);
        }
        if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)
            || (value >= 0x80 && value <= 0x9f))
            return false;
    }
    return true;
}

bool valid(const Observation &observation)
{
    if (observation.replaceRetainedTrack && observation.playback != Playback::Paused) return false;
    switch (observation.playback) {
    case Playback::Playing:
        return !observation.retainedTrack && observation.metadata && validMetadata(*observation.metadata);
    case Playback::Paused:
        return !observation.metadata && (!observation.retainedTrack || validMetadata(*observation.retainedTrack));
    case Playback::Waiting:
    case Playback::Unavailable:
        return !observation.metadata && !observation.retainedTrack;
    }
    return false;
}
} // namespace

bool validMetadata(const Metadata &metadata) noexcept
{
    return !metadata.title.empty() && validText(metadata.title) && validText(metadata.artist)
        && (!metadata.identity || (!metadata.identity->scope.empty() && !metadata.identity->value.empty()
            && validText(metadata.identity->scope) && validText(metadata.identity->value)))
        && (!metadata.origin || (!metadata.origin->provider.empty() && validText(metadata.origin->provider)
            && validText(metadata.origin->source))) && validArtwork(metadata.artwork);
}

bool TrackIdentity::operator==(const TrackIdentity &other) const noexcept
{ return scope == other.scope && value == other.value; }
bool ImageReference::operator==(const ImageReference &other) const noexcept
{ return kind == other.kind && location == other.location && version == other.version; }
bool TrackOrigin::operator==(const TrackOrigin &other) const noexcept
{ return provider == other.provider && source == other.source; }
bool Artwork::operator==(const Artwork &other) const noexcept
{ return status == other.status && image == other.image; }

bool validImageReference(const ImageReference &image) noexcept
{
    if (image.location.empty() || !validText(image.location) || !validText(image.version)) return false;
    const auto &path = image.location;
    switch (image.kind) {
    case ImageKind::Asset:
        // Host-owned opaque ID, not a path to resolve or execute.
        return path.find_first_of("/\\:") == std::string::npos && path != "." && path != "..";
    case ImageKind::Https:
        // Conservative transport boundary, not a URL parser or fetch approval.
        return path.rfind("https://", 0) == 0 && path.size() > 8
            && path[8] != '/' && path[8] != '?' && path[8] != '#'
            && path.find_first_of(" \\@") == std::string::npos;
    case ImageKind::LocalFile:
        // Absolute Windows drive paths only; UNC/device paths deferred to host.
        return path.size() > 3 && ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z'))
            && path[1] == ':' && (path[2] == '/' || path[2] == '\\')
            && path.find(':', 2) == std::string::npos;
    }
    return false;
}
bool validArtwork(const Artwork &artwork) noexcept
{
    switch (artwork.status) {
    case ArtworkStatus::Ready: return artwork.image && validImageReference(*artwork.image);
    case ArtworkStatus::Unknown:
    case ArtworkStatus::Missing:
    case ArtworkStatus::Pending:
    case ArtworkStatus::Failed: return !artwork.image;
    }
    return false;
}
std::optional<ImageReference> resolveArtwork(const Artwork &artwork,
    const std::optional<ImageReference> &profile, bool fallback)
{
    if (validArtwork(artwork) && artwork.status == ArtworkStatus::Ready) return artwork.image;
    if (fallback && profile && validImageReference(*profile)) return profile;
    return std::nullopt;
}
bool TrackState::operator==(const TrackState &other) const noexcept
{
    return title == other.title && artist == other.artist && identity == other.identity
        && artwork == other.artwork && origin == other.origin;
}

PlayingCore::PlayingCore()
    : owner_(std::this_thread::get_id()), snapshot_(std::make_shared<const Snapshot>())
{
}

SnapshotPtr PlayingCore::snapshot() const noexcept
{
    return std::atomic_load_explicit(&snapshot_, std::memory_order_acquire);
}

void PlayingCore::requireOwner() const
{
    if (std::this_thread::get_id() != owner_)
        throw std::logic_error("PlayingCore mutation requires the owner thread");
}

void PlayingCore::publish(bool running, Playback playback, std::optional<Metadata> metadata,
    std::shared_ptr<const TrackState> track, std::uint64_t trackRevision)
{
    const auto previous = snapshot();
    // Prepare fully before publication: failure leaves the old snapshot intact.
    SnapshotPtr next = std::make_shared<const Snapshot>(Snapshot{
        previous->revision + 1, running, playback, std::move(metadata), std::move(track), trackRevision});
    std::atomic_store_explicit(&snapshot_, std::move(next), std::memory_order_release);
}

Update PlayingCore::start()
{
    requireOwner();
    if (snapshot()->running)
        return Update::Unchanged;
    publish(true, Playback::Waiting, std::nullopt);
    return Update::Applied;
}

Update PlayingCore::stop()
{
    requireOwner();
    if (!snapshot()->running)
        return Update::Unchanged;
    publish(false, Playback::Waiting, std::nullopt);
    return Update::Applied;
}

Update PlayingCore::observe(const Observation &observation)
{
    requireOwner();
    const auto current = snapshot();
    if (!current->running)
        return Update::NotRunning;
    if (!valid(observation))
        return Update::InvalidObservation;
    auto metadata = observation.metadata;
    // Unknown means no cover information from this reader, not an instruction
    // to discard host enrichment. Explicit Missing/Failed still clear it.
    if (metadata && current->track && metadata->artwork.status == ArtworkStatus::Unknown
        && metadata->title == current->track->title && metadata->artist == current->track->artist
        && metadata->identity == current->track->identity && metadata->origin == current->track->origin)
        metadata->artwork = current->track->artwork;
    if (current->playback == observation.playback && current->metadata == metadata
        && (!observation.replaceRetainedTrack || observation.retainedTrack || !current->track)
        && (!observation.retainedTrack || (current->track && *current->track == *observation.retainedTrack)))
        return Update::Unchanged;
    std::shared_ptr<const TrackState> track;
    std::uint64_t token = 0;
    if (observation.playback == Playback::Playing) {
        track = std::make_shared<const TrackState>(*metadata);
        // Reuse token for presentation-only updates, never for a different identity.
        const auto &next = *track;
        const bool same = current->track && current->track->title == next.title
            && current->track->artist == next.artist && current->track->identity == next.identity
            && current->track->origin == next.origin;
        token = same ? current->trackRevision : ++trackTokens;
    } else if (observation.playback == Playback::Paused) {
        if (observation.retainedTrack) {
            track = std::make_shared<const TrackState>(*observation.retainedTrack);
            const bool same = current->track && current->track->title == track->title
                && current->track->artist == track->artist && current->track->identity == track->identity
                && current->track->origin == track->origin;
            token = same ? current->trackRevision : ++trackTokens;
        } else if (!observation.replaceRetainedTrack) { track = current->track; token = current->trackRevision; }
    }
    publish(true, observation.playback, std::move(metadata), std::move(track), token);
    return Update::Applied;
}

Update PlayingCore::updateArtwork(std::uint64_t expectedTrackRevision, const Artwork &artwork)
{
    requireOwner();
    const auto current = snapshot();
    if (!current->running) return Update::NotRunning;
    if (!validArtwork(artwork)) return Update::InvalidObservation;
    if (!current->track || expectedTrackRevision == 0 || expectedTrackRevision != current->trackRevision)
        return Update::StaleTrack;
    if (current->track->artwork == artwork) return Update::Unchanged;
    auto track = *current->track; track.artwork = artwork;
    auto active = current->metadata;
    if (active) active->artwork = artwork;
    publish(true, current->playback, std::move(active),
        std::make_shared<const TrackState>(std::move(track)), current->trackRevision);
    return Update::Applied;
}

} // namespace playing::core
