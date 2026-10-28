// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <thread>

namespace playing::core {

// Opaque, authoritative identity, scoped to a provider/library. Never derive it
// from title/artist or process pointers. Absence means the reader cannot supply it.
struct TrackIdentity {
    std::string scope;
    std::string value;
    bool operator==(const TrackIdentity &other) const noexcept;
};
enum class ArtworkStatus { Unknown, Missing, Pending, Ready, Failed };
enum class ImageKind { LocalFile, Https, Asset };
struct ImageReference {
    ImageKind kind = ImageKind::Asset;
    std::string location;
    // Opaque cache version supplied by the host; core performs no image I/O.
    std::string version;
    bool operator==(const ImageReference &other) const noexcept;
};
struct Artwork {
    ArtworkStatus status = ArtworkStatus::Unknown;
    std::optional<ImageReference> image;
    bool operator==(const Artwork &other) const noexcept;
};
struct TrackOrigin {
    std::string provider;
    std::string source;
    bool operator==(const TrackOrigin &other) const noexcept;
};
struct TrackState {
    std::string title;
    std::string artist;
    std::optional<TrackIdentity> identity = std::nullopt;
    Artwork artwork = {};
    std::optional<TrackOrigin> origin = std::nullopt;
    bool operator==(const TrackState &other) const noexcept;
};
using Metadata = TrackState; // Source compatibility for phase-05 readers.
bool validImageReference(const ImageReference &image) noexcept;
bool validArtwork(const Artwork &artwork) noexcept;
// Consumer policy: profile fallback is independent of the shared DJ session.
// Missing/pending/failed covers use the configured profile, if valid. No I/O.
std::optional<ImageReference> resolveArtwork(const Artwork &artwork,
    const std::optional<ImageReference> &streamerProfile, bool useProfileFallback = true);
// Shared boundary validation; no normalization, allocations or publication.
bool validMetadata(const Metadata &metadata) noexcept;

enum class Playback { Waiting, Playing, Paused, Unavailable };

// A reader/adapter submits an observation only when it has new information.
// Playing requires metadata. Other states require no metadata; a pause must
// never carry the previous song as though it were still playing.
struct Observation {
    Playback playback = Playback::Waiting;
    std::optional<Metadata> metadata;
    // Complete remote Paused snapshots may include a known retained track,
    // without publishing active text. Other playback states forbid this field.
    std::optional<TrackState> retainedTrack = std::nullopt;
    bool replaceRetainedTrack = false; // Complete Paused snapshot, including explicit absence.
};

struct Snapshot {
    std::uint64_t revision = 0;
    bool running = false;
    Playback playback = Playback::Waiting;
    std::optional<Metadata> metadata;
    // Canonical immutable track; retained on explicit Paused, cleared on Waiting,
    // Unavailable/stop. metadata remains the active text compatibility view.
    std::shared_ptr<const TrackState> track;
    std::uint64_t trackRevision = 0;
};
using SnapshotPtr = std::shared_ptr<const Snapshot>;

enum class Update { Applied, Unchanged, NotRunning, InvalidObservation, StaleTrack };

class PlayingCore final {
public:
    static constexpr std::size_t maxMetadataBytes = 4096; // Per field, UTF-8.

    PlayingCore();
    PlayingCore(const PlayingCore &) = delete;
    PlayingCore &operator=(const PlayingCore &) = delete;
    PlayingCore(PlayingCore &&) = delete;
    PlayingCore &operator=(PlayingCore &&) = delete;

    // Mutations must occur on the constructing thread. Future adapters marshal
    // observations to that owner; no provider/UI/renderer is owned by the core.
    Update start();
    Update stop();
    Update observe(const Observation &observation);
    // Late image results are accepted only for this track token. No fetching.
    Update updateArtwork(std::uint64_t expectedTrackRevision, const Artwork &artwork);

    // Consumers share the same immutable publication and may read from any
    // thread. Retained snapshots survive subsequent updates and core lifetime.
    SnapshotPtr snapshot() const noexcept;

private:
    void requireOwner() const;
    void publish(bool running, Playback playback, std::optional<Metadata> metadata,
        std::shared_ptr<const TrackState> track = {}, std::uint64_t trackRevision = 0);
    const std::thread::id owner_;
    SnapshotPtr snapshot_;
};

} // namespace playing::core
