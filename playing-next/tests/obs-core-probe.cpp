// SPDX-License-Identifier: MIT
#include "playing/core/playing-core.hpp"
#include "playing/providers/provider-registry.hpp"
#include "playing/providers/virtualdj-history.hpp"
#include "playing/providers/rekordbox-history.hpp"
#include "playing/providers/virtualdj-live.hpp"
#include <filesystem>
#include "playing/providers/serato-live.hpp"
#include <atomic>
#include "playing/providers/rekordbox-live.hpp"
#include "playing/providers/network-provider.hpp"
#include <chrono>

// Test-only entry point. No OBS API, provider, UI or C++ types cross the FFI ABI.
extern "C" __declspec(dllexport) int playing_core_smoke_test() noexcept
{
    using namespace playing::core;
    try {
        PlayingCore core;
        if (core.start() != Update::Applied) return 1;
        const Observation song{Playback::Playing, Metadata{u8"Jóga 🎵", u8"Björk"}};
        if (core.observe(song) != Update::Applied) return 2;
        const auto retained = core.snapshot();
        if (retained->metadata->artist != u8"Björk") return 3;
        if (core.observe(song) != Update::Unchanged || core.snapshot() != retained) return 4;
        const auto token = retained->trackRevision;
        if (!token || !retained->track) return 8;
        const ImageReference profile{ImageKind::Asset, "streamer-profile", "1"};
        if (!(resolveArtwork(retained->track->artwork, profile) == std::optional<ImageReference>{profile})) return 9;
        const Artwork cover{ArtworkStatus::Ready, ImageReference{ImageKind::Asset, "cover", "1"}};
        if (core.updateArtwork(token, cover) != Update::Applied) return 10;
        if (core.observe({Playback::Paused, std::nullopt}) != Update::Applied) return 5;
        if (core.snapshot()->metadata || !retained->metadata) return 6;
        if (!core.snapshot()->track || !(core.snapshot()->track->artwork == cover)) return 11;
        if (core.updateArtwork(token, {ArtworkStatus::Failed, std::nullopt}) != Update::Applied) return 12;
        if (!(resolveArtwork(core.snapshot()->track->artwork, profile) == std::optional<ImageReference>{profile})) return 13;
        if (core.stop() != Update::Applied || core.snapshot()->running) return 7;
        if (core.snapshot()->track) return 14;
        namespace net = playing::providers::network;
        using namespace playing::providers;
        ProviderRegistry registry; auto channel = std::make_shared<net::Channel>(5);
        if (net::registerProvider(registry, channel) != Registration::Registered) return 15;
        const Request remote{"network", Mode::Automatic, ""};
        const auto a = registry.acquire(remote), b = registry.acquire(remote);
        if (!channel->connect("obs-probe-epoch", 0)) return 16;
        core.start(); core.observe(song); core.observe({Playback::Paused, std::nullopt});
        const auto encoded = net::encode(net::fromSnapshot(*core.snapshot(), "obs-probe-epoch", 1));
        if (!encoded || channel->receive(*encoded, 1) != net::Ingress::Accepted) return 17;
        if (registry.refresh(remote).result != Refresh::Applied) return 18;
        const auto received = registry.snapshot(a.token);
        if (received != registry.snapshot(b.token) || !received->track || received->metadata
            || received->playback != Playback::Paused) return 19;
        if (!(resolveArtwork(received->track->artwork, profile) == std::optional<ImageReference>{profile})) return 20;
        if (!channel->expire(6) || registry.refresh(remote).result != Refresh::Unavailable) return 21;
        if (registry.snapshot(a.token)->track) return 22;
        registry.release(a.token); registry.release(b.token);
        return 0;
    } catch (...) {
        return 255;
    }
}

namespace {
struct ProbeCounts { int starts = 0, samples = 0, stops = 0; };
class ProbeProvider final : public playing::providers::IDjProvider {
public:
    explicit ProbeProvider(ProbeCounts &counts) : counts_(counts) {}
    void start() override { ++counts_.starts; }
    void stop() noexcept override { ++counts_.stops; }
    playing::providers::Sample sample() override
    {
        ++counts_.samples;
        return {playing::providers::SampleKind::Observation,
            playing::core::Observation{playing::core::Playback::Playing,
                playing::core::Metadata{u8"Jóga", u8"Björk"}}};
    }
private:
    ProbeCounts &counts_;
};
}

extern "C" __declspec(dllexport) int playing_provider_smoke_test() noexcept
{
    using namespace playing::providers;
    try {
        ProbeCounts counts;
        ProviderRegistry registry;
        registry.registerProvider({"probe", "Test fixture", true, false},
            [&counts](const Request &) { return std::make_unique<ProbeProvider>(counts); });
        const Request request{"probe"};
        const auto a = registry.acquire(request), b = registry.acquire(request);
        if (a.result != Acquisition::Acquired || b.result != Acquisition::Acquired
            || counts.starts != 1) return 1;
        if (registry.refresh(request).result != Refresh::Applied || counts.samples != 1) return 2;
        if (registry.snapshot(a.token) != registry.snapshot(b.token)
            || registry.snapshot(a.token)->metadata->artist != u8"Björk") return 3;
        if (!registry.release(a.token) || counts.stops != 0) return 4;
        if (!registry.release(b.token) || counts.stops != 1) return 5;
        if (registry.refresh(request).result != Refresh::UnknownSession || counts.samples != 1) return 6;
        return 0;
    } catch (...) {
        return 255;
    }
}

namespace {
struct HistoryProbe {
    playing::providers::ProviderRegistry registry;
    playing::providers::Request request;
    playing::providers::ConsumerToken first, second;
};
std::unique_ptr<HistoryProbe> historyProbe;
std::string historyProbeError;
std::unique_ptr<HistoryProbe> liveProbe;
std::string liveProbeError;
std::unique_ptr<HistoryProbe> seratoProbe;
std::atomic<unsigned> seratoPort{0};
std::unique_ptr<HistoryProbe> rekordboxLiveProbe;
std::string rekordboxLiveProbeError;
}

extern "C" __declspec(dllexport) int playing_rekordbox_live_probe_start(unsigned pid, unsigned long long module) noexcept {
    using namespace playing::providers;
    try {
        if (!pid || rekordboxLiveProbe) return 2;
        auto probe = std::make_unique<HistoryProbe>(); probe->request = {"rekordbox"};
        probe->registry.registerProvider({"rekordbox", "Isolated memory fixture", true, false}, [pid, module](const Request &request) {
            return rekordbox::makeLive(request, [pid, module] {
                virtualdj::Connection connection; connection.memory = virtualdj::openReadOnly(pid);
                if (!connection.memory || !connection.memory->alive()) { connection.memory.reset(); connection.issue = virtualdj::ProcessIssue::Missing; return connection; }
                if (std::filesystem::path(connection.memory->imagePath()).filename() != L"playing-rekordbox-live-probe-fixture.exe") { connection.memory.reset(); connection.issue = virtualdj::ProcessIssue::Access; return connection; }
                connection.issue = virtualdj::ProcessIssue::None; connection.module = module; connection.version = "7.2.19.0"; return connection;
            });
        });
        const auto a = probe->registry.acquire(probe->request), b = probe->registry.acquire(probe->request);
        if (a.result != Acquisition::Acquired || b.result != Acquisition::Acquired) return 3;
        probe->first = a.token; probe->second = b.token; rekordboxLiveProbe = std::move(probe); return 0;
    } catch (...) { return 255; }
}
extern "C" __declspec(dllexport) int playing_rekordbox_live_probe_step(unsigned stage) noexcept {
    using namespace playing::providers;
    try {
        if (!rekordboxLiveProbe || stage > 4) return 3;
        auto &probe = *rekordboxLiveProbe; const auto result = probe.registry.refresh(probe.request);
        rekordboxLiveProbeError = "refresh=" + std::to_string(static_cast<int>(result.result)) + "; issue=" + std::to_string(static_cast<int>(result.issue));
        if (result.result == Refresh::Pending) return 1;
        const auto a = probe.registry.snapshot(probe.first), b = probe.registry.snapshot(probe.second);
        if (a != b || probe.registry.activeRequests().size() != 1 || probe.registry.consumerCount() != 2) return 4;
        if (stage == 4) {
            if ((result.result != Refresh::Unavailable && result.result != Refresh::Unchanged)
                || result.issue != ProviderIssue::ProcessMissing || a->metadata || a->playback != playing::core::Playback::Unavailable) return 1;
            if (!probe.registry.release(probe.first) || probe.registry.activeRequests().size() != 1) return 5;
            if (!probe.registry.release(probe.second) || !probe.registry.activeRequests().empty()) return 6;
            return 0;
        }
        if (result.result == Refresh::TemporaryFailure && result.issue == ProviderIssue::SnapshotChanged) return 1;
        if (result.result != Refresh::Applied && result.result != Refresh::Unchanged) return 7;
        if (stage == 2) { if (a->metadata || a->playback != playing::core::Playback::Waiting) return 1; }
        else {
            const auto title = stage == 0 ? u8"Canción 1" : stage == 1 ? u8"Canción 4" : u8"Canción 2";
            if (!a->metadata || a->metadata->title != title) return 1;
            if (a->metadata->artist != u8"Björk") return 8;
        }
        return 2;
    } catch (...) { return 255; }
}
extern "C" __declspec(dllexport) void playing_rekordbox_live_probe_stop() noexcept { rekordboxLiveProbe.reset(); }
extern "C" __declspec(dllexport) const char *playing_rekordbox_live_probe_error() noexcept { return rekordboxLiveProbeError.c_str(); }

extern "C" __declspec(dllexport) unsigned playing_serato_probe_port() noexcept { return seratoPort.load(); }
extern "C" __declspec(dllexport) int playing_serato_probe_start() noexcept {
    using namespace playing::providers;
    try {
        if (seratoProbe) return 2;
        auto probe = std::make_unique<HistoryProbe>(); probe->request = {"serato"};
        SeratoOptions options; options.advertise = false; options.boundPort = [](unsigned value) { seratoPort = value; };
        probe->registry.registerProvider({"serato", "Isolated local peer", true, false}, [options](const Request &request) { return makeSeratoLive(request, options); });
        const auto a = probe->registry.acquire(probe->request), b = probe->registry.acquire(probe->request);
        if (a.result != Acquisition::Acquired || b.result != Acquisition::Acquired) return 3;
        probe->first = a.token; probe->second = b.token; seratoProbe = std::move(probe); return 0;
    } catch (...) { return 255; }
}
extern "C" __declspec(dllexport) int playing_serato_probe_step(unsigned stage) noexcept {
    using namespace playing::providers;
    try {
        if (!seratoProbe || stage > 4) return 3;
        auto &probe = *seratoProbe; const auto result = probe.registry.refresh(probe.request);
        if (result.result == Refresh::Pending) return 1;
        const auto a = probe.registry.snapshot(probe.first), b = probe.registry.snapshot(probe.second);
        if (a != b || probe.registry.activeRequests().size() != 1 || probe.registry.consumerCount() != 2) return 4;
        if (stage == 4) {
            if (result.result != Refresh::Unavailable || result.issue != ProviderIssue::ConnectionLost || a->metadata) return 1;
            if (!probe.registry.release(probe.first) || probe.registry.activeRequests().size() != 1) return 5;
            if (!probe.registry.release(probe.second) || !probe.registry.activeRequests().empty() || seratoPort) return 6;
            return 0;
        }
        if (result.result != Refresh::Applied && result.result != Refresh::Unchanged) return 7;
        if (stage == 2) { if (a->metadata || a->playback != playing::core::Playback::Waiting) return 1; }
        else if (!a->metadata || a->metadata->title != (stage == 1 ? "Track 2" : "Track 1")) return 1;
        return 2;
    } catch (...) { return 255; }
}
extern "C" __declspec(dllexport) void playing_serato_probe_stop() noexcept { seratoProbe.reset(); }

// Isolated test ABI: only connects to the specifically named fixture executable.
// Production discovery/version gating remains separate and is not bypassed there.
extern "C" __declspec(dllexport) int playing_live_probe_start(unsigned pid, unsigned long long module) noexcept
{
    using namespace playing::providers;
    try {
        if (!pid || liveProbe) return 2;
        auto probe = std::make_unique<HistoryProbe>(); probe->request = {"virtualdj"};
        probe->registry.registerProvider({"virtualdj", "Isolated memory fixture", true, false},
            [pid, module](const Request &request) {
                return std::make_unique<VirtualDjLive>(request, [pid, module] {
                    virtualdj::Connection connection; connection.memory = virtualdj::openReadOnly(pid);
                    if (!connection.memory || std::filesystem::path(connection.memory->imagePath()).filename() != L"playing-virtualdj-probe-fixture.exe") {
                        connection.memory.reset(); connection.issue = virtualdj::ProcessIssue::Access; return connection;
                    }
                    connection.module = module; connection.version = virtualdj::verifiedVersion;
                    connection.issue = virtualdj::ProcessIssue::None; return connection;
                });
            });
        const auto a = probe->registry.acquire(probe->request), b = probe->registry.acquire(probe->request);
        if (a.result != Acquisition::Acquired || b.result != Acquisition::Acquired) return 3;
        probe->first = a.token; probe->second = b.token; liveProbe = std::move(probe); return 0;
    } catch (const std::exception &error) { liveProbeError = error.what(); return 255; }
    catch (...) { return 255; }
}
extern "C" __declspec(dllexport) int playing_live_probe_step(unsigned stage) noexcept
{
    using namespace playing::providers;
    try {
        if (!liveProbe || stage > 4) return 3;
        auto &probe = *liveProbe;
        const auto result = probe.registry.refresh(probe.request);
        if (result.result == Refresh::Pending) return 1;
        if (result.result != Refresh::Applied && result.result != Refresh::Unchanged) return 4;
        const auto a = probe.registry.snapshot(probe.first), b = probe.registry.snapshot(probe.second);
        if (a != b || probe.registry.activeRequests().size() != 1 || probe.registry.consumerCount() != 2) return 5;
        if (stage == 3) {
            if (a->metadata || a->playback != playing::core::Playback::Waiting) return 1;
        } else {
            const auto title = stage == 0 ? "Track 1" : stage == 4 ? "Track 2" : "Track 4";
            if (!a->metadata || a->metadata->title != title) return 1;
            if (a->metadata->artist != u8"Björk") return 6;
        }
        if (stage < 4) return 2;
        if (!probe.registry.release(probe.first) || probe.registry.activeRequests().size() != 1) return 7;
        if (!probe.registry.release(probe.second) || !probe.registry.activeRequests().empty()) return 8;
        if (probe.registry.refresh(probe.request).result != Refresh::UnknownSession) return 9;
        return 0;
    } catch (const std::exception &error) { liveProbeError = error.what(); return 255; }
    catch (...) { return 255; }
}
extern "C" __declspec(dllexport) void playing_live_probe_stop() noexcept { liveProbe.reset(); }
extern "C" __declspec(dllexport) const char *playing_live_probe_error() noexcept { return liveProbeError.c_str(); }

// Async test-only ABI: Lua starts a real file reader, collects on the OBS owner
// thread, and stops it. No mocks or disk IO inside the owner-thread step.
extern "C" __declspec(dllexport) int playing_history_probe_start(const char *filename) noexcept
{
    using namespace playing::providers;
    try {
        if (!filename || historyProbe) return 2;
        auto probe = std::make_unique<HistoryProbe>();
        probe->request = {"virtualdj", Mode::History, filename};
        if (registerVirtualDjHistory(probe->registry) != Registration::Registered) return 3;
        const auto a = probe->registry.acquire(probe->request), b = probe->registry.acquire(probe->request);
        if (a.result != Acquisition::Acquired || b.result != Acquisition::Acquired) return 4;
        probe->first = a.token; probe->second = b.token;
        historyProbe = std::move(probe); return 0;
    } catch (const std::exception &error) { historyProbeError = error.what(); return 255; }
    catch (...) { return 255; }
}
extern "C" __declspec(dllexport) int playing_history_probe_step() noexcept
{
    using namespace playing::providers;
    try {
        if (!historyProbe) return 2;
        auto &probe = *historyProbe;
        const auto result = probe.registry.refresh(probe.request);
        if (result.result == Refresh::Pending) return 1;
        if (result.result != Refresh::Applied && result.result != Refresh::Unchanged) return 3;
        const auto first = probe.registry.snapshot(probe.first), second = probe.registry.snapshot(probe.second);
        if (first != second || !first->metadata || first->metadata->title != u8"Jóga 🎵"
            || first->metadata->artist != u8"Björk") return 4;
        if (!probe.registry.release(probe.first) || probe.registry.activeRequests().size() != 1) return 5;
        if (!probe.registry.release(probe.second) || !probe.registry.activeRequests().empty()) return 6;
        return 0;
    } catch (const std::exception &error) { historyProbeError = error.what(); return 255; }
    catch (...) { return 255; }
}
extern "C" __declspec(dllexport) void playing_history_probe_stop() noexcept { historyProbe.reset(); }
extern "C" __declspec(dllexport) const char *playing_history_probe_error() noexcept { return historyProbeError.c_str(); }

namespace {
std::unique_ptr<HistoryProbe> rekordboxProbe;
std::string rekordboxProbeError;
}
extern "C" __declspec(dllexport) int playing_rekordbox_probe_start(const char *filename, const char *library) noexcept
{
    using namespace playing::providers;
    try {
        rekordboxProbeError.clear();
        if (!filename || !library || rekordboxProbe) return 2;
        auto probe = std::make_unique<HistoryProbe>();
        probe->request = {"rekordbox", Mode::History, filename};
        if (registerRekordboxHistory(probe->registry, library) != Registration::Registered) return 3;
        const auto a = probe->registry.acquire(probe->request), b = probe->registry.acquire(probe->request);
        if (a.result != Acquisition::Acquired || b.result != Acquisition::Acquired) return 4;
        probe->first = a.token; probe->second = b.token;
        rekordboxProbe = std::move(probe); return 0;
    } catch (const std::exception &e) { rekordboxProbeError = e.what(); return 255; }
    catch (...) { return 255; }
}
extern "C" __declspec(dllexport) int playing_rekordbox_probe_step(int expectWal) noexcept
{
    using namespace playing::providers;
    try {
        if (!rekordboxProbe) return 3;
        auto &probe = *rekordboxProbe;
        const auto result = probe.registry.refresh(probe.request);
        if (result.result == Refresh::Pending) return 1;
        if (result.result != Refresh::Applied && result.result != Refresh::Unchanged) {
            rekordboxProbeError = "Refresh=" + std::to_string(static_cast<int>(result.result))
                + "; issue=" + std::to_string(static_cast<int>(result.issue)); return 4;
        }
        const auto a = probe.registry.snapshot(probe.first), b = probe.registry.snapshot(probe.second);
        if (a != b || !a->metadata || a->metadata->artist != u8"Björk") return 5;
        if (!expectWal) return a->metadata->title == u8"Jóga 🎵" ? 2 : 6;
        if (a->metadata->title == u8"Jóga 🎵") return 1; // External fixture writer has not committed yet.
        if (a->metadata->title != u8"Next WAL 🎵") return 7;
        if (!probe.registry.release(probe.first) || probe.registry.activeRequests().size() != 1) return 8;
        if (!probe.registry.release(probe.second) || !probe.registry.activeRequests().empty()) return 9;
        return 0;
    } catch (const std::exception &e) { rekordboxProbeError = e.what(); return 255; }
    catch (...) { return 255; }
}
extern "C" __declspec(dllexport) void playing_rekordbox_probe_stop() noexcept { rekordboxProbe.reset(); }
extern "C" __declspec(dllexport) const char *playing_rekordbox_probe_error() noexcept { return rekordboxProbeError.c_str(); }

// Test-only bounded bridge. TLS authentication happens in an external Receiver;
// Lua supplies its atomic fixture frame. No Qt DLL is loaded into OBS.
namespace {
struct CompanionProbe {
    playing::providers::ProviderRegistry registry;
    std::shared_ptr<playing::providers::network::Channel> channel=std::make_shared<playing::providers::network::Channel>(60);
    playing::providers::ConsumerToken first,second;
    std::chrono::steady_clock::time_point start=std::chrono::steady_clock::now();
};
std::unique_ptr<CompanionProbe> companionProbe;
}
extern "C" __declspec(dllexport) int playing_companion_probe_start() noexcept {
    using namespace playing::providers;
    try {
        if(companionProbe) return 1;
        auto p=std::make_unique<CompanionProbe>();
        if(network::registerProvider(p->registry,p->channel)!=Registration::Registered || !p->channel->connect("bridge",0)) return 2;
        const Request request{"network",Mode::Automatic,""}; p->first=p->registry.acquire(request).token; p->second=p->registry.acquire(request).token;
        if(!p->first || !p->second) return 3; companionProbe=std::move(p); return 0;
    } catch(...) {return 255;}
}
extern "C" __declspec(dllexport) int playing_companion_probe_step(const char *frame,unsigned size,unsigned stage) noexcept {
    using namespace playing::providers; using namespace playing::core;
    try {
        if(!companionProbe || !frame || size>network::maxFrameBytes || stage>2) return 3;
        auto &p=*companionProbe; const double now=std::chrono::duration<double>(std::chrono::steady_clock::now()-p.start).count();
        const auto ingress=p.channel->receive(std::string_view(frame,size),now);
        if(ingress!=network::Ingress::Accepted && ingress!=network::Ingress::Replayed) return 4;
        p.registry.refresh({"network",Mode::Automatic,""}); const auto a=p.registry.snapshot(p.first),b=p.registry.snapshot(p.second);
        if(a!=b) return 5;
        if(stage<2) {
            if(a->playback!=Playback::Playing || !a->track || !a->metadata) return 1;
            if(a->metadata->artist!=u8"Björk") return 6;
            if(a->metadata->title!=(stage==0?u8"Jóga 🎵":u8"Next 🎵")) return 1;
            if(!a->track->origin || a->track->origin->provider!="virtualdj") return 7;
            if(resolveArtwork(a->track->artwork,ImageReference{ImageKind::Asset,"streamer-profile",""})->location!="streamer-profile") return 8;
        } else if(a->playback!=Playback::Unavailable || a->track || a->metadata) return 1;
        return 2;
    } catch(...) {return 255;}
}
extern "C" __declspec(dllexport) int playing_companion_probe_stop() noexcept {
    try {if(!companionProbe) return 0; auto &p=*companionProbe; const bool ok=p.registry.release(p.first) && p.registry.release(p.second) && p.registry.activeRequests().empty(); companionProbe.reset(); return ok?0:1;} catch(...) {return 255;}
}
