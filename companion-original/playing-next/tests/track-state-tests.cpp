// SPDX-License-Identifier: MIT
#include "playing/providers/provider-registry.hpp"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <atomic>
using namespace playing::core;
using namespace playing::providers;
namespace {
unsigned checks = 0;
void check(bool value) { ++checks; if (!value) throw std::runtime_error("Track State check " + std::to_string(checks)); }
TrackState song(const char *id = "1") {
    TrackState state{u8"Canción 🎵", u8"Artista"};
    state.identity = TrackIdentity{"library-A", id};
    return state;
}
const ImageReference profile{ImageKind::Asset, "streamer-avatar", "v1"};
const Artwork cover{ArtworkStatus::Ready, ImageReference{ImageKind::Https, "https://example.test/cover.png", "v2"}};
struct Fixture final : IDjProvider {
    Sample next{SampleKind::Observation, Observation{Playback::Playing, song()}};
    void start() override {} void stop() noexcept override {}
    Sample sample() override { return next; }
};
void coreContract() {
    PlayingCore core;
    check(core.updateArtwork(1, cover) == Update::NotRunning);
    core.start(); check(!core.snapshot()->track && core.snapshot()->trackRevision == 0);
    check(core.observe({Playback::Playing, song()}) == Update::Applied);
    const auto first = core.snapshot(); const auto token = first->trackRevision;
    check(token != 0 && first->track && *first->track == *first->metadata);
    check(core.observe({Playback::Playing, song()}) == Update::Unchanged && core.snapshot() == first);
    check(core.updateArtwork(token, cover) == Update::Applied);
    check(core.snapshot()->trackRevision == token && core.snapshot()->metadata->artwork == cover);
    check(first->track->artwork.status == ArtworkStatus::Unknown);
    const auto image = core.snapshot();
    check(core.observe({Playback::Playing, song()}) == Update::Unchanged && core.snapshot() == image);
    check(core.updateArtwork(token, cover) == Update::Unchanged && core.snapshot() == image);
    check(core.observe({Playback::Paused, std::nullopt}) == Update::Applied);
    check(!core.snapshot()->metadata && core.snapshot()->track && core.snapshot()->trackRevision == token);
    check(core.updateArtwork(token, {ArtworkStatus::Failed, std::nullopt}) == Update::Applied);
    check(!core.snapshot()->metadata && core.snapshot()->track->artwork.status == ArtworkStatus::Failed);
    check(core.observe({Playback::Playing, song("2")}) == Update::Applied);
    check(core.snapshot()->trackRevision != token); // Same title/artist, different authoritative ID.
    const auto second = core.snapshot();
    check(core.updateArtwork(token, cover) == Update::StaleTrack && core.snapshot() == second);
    check(core.updateArtwork(second->trackRevision, {ArtworkStatus::Ready, std::nullopt}) == Update::InvalidObservation);
    check(core.snapshot() == second);
    auto scoped = song("2"); scoped.identity->scope = "library-B";
    check(core.observe({Playback::Playing, scoped}) == Update::Applied);
    check(core.snapshot()->trackRevision != second->trackRevision);
    core.observe({Playback::Playing, song("2")});
    const auto scopedBack = core.snapshot();
    auto invalid = song(); invalid.identity->value.clear();
    check(core.observe({Playback::Playing, invalid}) == Update::InvalidObservation && core.snapshot() == scopedBack);
    invalid = song(); invalid.identity->scope = std::string(4097, 'a'); check(!validMetadata(invalid));
    invalid = song(); invalid.origin = TrackOrigin{"", "history"}; check(!validMetadata(invalid));
    invalid = song(); invalid.artwork = {ArtworkStatus::Pending, profile}; check(!validMetadata(invalid));
    auto unknown = TrackState{"same", "artist"}; check(validMetadata(unknown) && !unknown.identity);
    check(core.observe({Playback::Waiting, std::nullopt}) == Update::Applied && !core.snapshot()->track);
    check(core.updateArtwork(second->trackRevision, cover) == Update::StaleTrack);
    core.observe({Playback::Playing, song("2")}); const auto next = core.snapshot()->trackRevision;
    check(next != second->trackRevision);
    core.observe({Playback::Unavailable, std::nullopt}); check(!core.snapshot()->track);
    core.observe({Playback::Playing, song()}); const auto old = core.snapshot()->trackRevision;
    core.stop(); check(!core.snapshot()->track); core.start(); core.observe({Playback::Playing, song()});
    check(core.snapshot()->trackRevision != old);
    std::atomic<bool> rejected{false}; std::thread worker([&] {
        try { core.updateArtwork(core.snapshot()->trackRevision, cover); } catch (const std::logic_error &) { rejected = true; }
    }); worker.join(); check(rejected);
    PlayingCore other; other.start(); other.observe({Playback::Playing, song()});
    check(other.updateArtwork(core.snapshot()->trackRevision, cover) == Update::StaleTrack);
}
void images() {
    check(validImageReference(profile)); check(validArtwork(cover));
    check(validImageReference({ImageKind::LocalFile, "C:/images/avatar.png", ""}));
    for (const auto &bad : {ImageReference{ImageKind::Https,"http://example.test/x",""},
        ImageReference{ImageKind::Https,"https://",""}, ImageReference{ImageKind::Https,"https://u@host/x",""},
        ImageReference{ImageKind::LocalFile,"avatar.png",""}, ImageReference{ImageKind::LocalFile,"C:/x:stream",""},
        ImageReference{ImageKind::Asset,"../x",""}, ImageReference{ImageKind::Asset,"",""},
        ImageReference{static_cast<ImageKind>(99),"x",""}}) check(!validImageReference(bad));
    for (const auto status : {ArtworkStatus::Unknown, ArtworkStatus::Missing, ArtworkStatus::Pending, ArtworkStatus::Failed}) {
        const Artwork missing{status, std::nullopt};
        check(resolveArtwork(missing, profile) == std::optional<ImageReference>{profile});
        check(!resolveArtwork(missing, profile, false)); check(!resolveArtwork(missing, std::nullopt));
    }
    check(resolveArtwork(cover, profile) == cover.image);
    check(!resolveArtwork({}, ImageReference{ImageKind::Asset,"../bad",""}));
    check(resolveArtwork({}, ImageReference{ImageKind::Asset,"other-profile",""})->location == "other-profile");
    check(!validArtwork({static_cast<ArtworkStatus>(99), std::nullopt}));
    check(!validImageReference({ImageKind::Asset,std::string("bad\0image",9),""}));
}
void registryContract() {
    ProviderRegistry registry; Fixture *fixture = nullptr;
    registry.registerProvider({"fixture", "Fixture", true, false}, [&](const Request &) {
        auto value = std::make_unique<Fixture>(); fixture = value.get(); return value;
    });
    const Request request{"fixture", Mode::Automatic, ""};
    const auto a = registry.acquire(request), b = registry.acquire(request);
    check(registry.refresh(request).result == Refresh::Applied);
    const auto first = registry.snapshot(a.token); const auto token = first->trackRevision;
    check(first->track->origin->provider == "fixture" && first->track->origin->source == "automatic");
    check(registry.updateArtwork(request, token, cover) == Update::Applied);
    check(registry.snapshot(a.token) == registry.snapshot(b.token));
    fixture->next = {SampleKind::TemporaryFailure, std::nullopt, ProviderIssue::ReadError};
    const auto image = registry.snapshot(a.token); check(registry.refresh(request).snapshot == image);
    fixture->next = {SampleKind::Pending}; check(registry.refresh(request).snapshot == image);
    fixture->next = {SampleKind::Observation, Observation{Playback::Playing, song()}};
    fixture->next.observation->metadata->origin = TrackOrigin{"wrong", "automatic"};
    check(registry.refresh(request).result == Refresh::InvalidSample && registry.snapshot(a.token) == image);
    registry.release(a.token); registry.release(b.token);
    check(registry.updateArtwork(request, token, cover) == Update::StaleTrack);
    const auto c = registry.acquire(request); registry.refresh(request);
    check(registry.updateArtwork(request, token, cover) == Update::StaleTrack);
    registry.release(c.token);
}
}
int main() {
    try { coreContract(); images(); registryContract(); std::cout << checks << " Track State checks; identity, immutable artwork, stale results, profile fallback and shared registry\n"; return 0; }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
