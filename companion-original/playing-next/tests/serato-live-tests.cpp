// SPDX-License-Identifier: MIT
#include "serato-socket-fixture.hpp"
#include "playing/providers/serato-live.hpp"
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <Windows.h>
using namespace playing::providers;
template<class Predicate> RefreshResult collect(ProviderRegistry &registry, const Request &request, Predicate predicate) {
    for (unsigned i = 0; i < 1000; ++i) {
        auto result = registry.refresh(request); if (predicate(result)) return result;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    throw std::runtime_error("Serato fixture timeout");
}
int main() {
    unsigned checks = 0;
    auto check = [&](bool ok) { if (!ok) throw std::runtime_error("check " + std::to_string(checks + 1)); ++checks; };
    try {
        std::atomic<unsigned> port{0}, starts{0}, factories{0};
        const auto owner = std::this_thread::get_id(); std::atomic<bool> onWorker{true};
        SeratoOptions options; options.advertise = false;
        options.boundPort = [&](unsigned value) { port = value; if (value) ++starts; if (std::this_thread::get_id() == owner) onWorker = false; };
        ProviderRegistry registry; const Request request{"serato"};
        check(registry.registerProvider({"serato", "Fixture", true, false}, [&](const Request &r) { ++factories; return makeSeratoLive(r, options); }) == Registration::Registered);
        check(port == 0 && starts == 0); const auto a = registry.acquire(request), b = registry.acquire(request);
        check(a.result == Acquisition::Acquired && b.result == Acquisition::Acquired); check(factories == 1); check(port == 0);
        for (unsigned i = 0; i < 500 && !port; ++i) { registry.refresh(request); std::this_thread::sleep_for(std::chrono::milliseconds(2)); }
        check(port > 0 && starts == 1); check(onWorker); check(registry.consumerCount() == 2);
        SeratoClient client(port); client.authorize();
        bool authorized = false;
        for (unsigned i = 0; i < 500 && !authorized; ++i) {
            registry.refresh(request); for (const auto &message : client.receive()) if (message.path == "/StreamMgmt/Authorize/Response") authorized = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        check(authorized); client.pair(); client.tracks();
        auto result = collect(registry, request, [](const RefreshResult &r) { return r.snapshot && r.snapshot->metadata; });
        check(result.snapshot->metadata->title == "Track 1"); check(registry.snapshot(a.token) == registry.snapshot(b.token));
        const auto fixture=std::filesystem::temp_directory_path()/std::filesystem::path("playing-serato-cover-"+std::to_string(GetCurrentProcessId())+".mp3");check(!std::filesystem::exists(fixture));
        const auto logo=std::filesystem::path(__FILE__).parent_path().parent_path().parent_path()/L"obs-playing/djemimas-logo.png";std::ifstream png(logo,std::ios::binary);check(bool(png));const std::string picture((std::istreambuf_iterator<char>(png)),{});
        std::string body(1,'\0');body+="image/png";body+='\0';body+='\3';body+='\0';body+=picture;
        std::string apic="APIC";for(int shift=24;shift>=0;shift-=8)apic+=char((body.size()>>shift)&255);apic.append(2,'\0');apic+=body;
        std::string id3("ID3\3\0\0",6);for(int shift=21;shift>=0;shift-=7)id3+=char((apic.size()>>shift)&127);id3+=apic;
        {std::ofstream out(fixture,std::ios::binary);out.write(id3.data(),std::streamsize(id3.size()));check(bool(out));}
        client.sendMessage({"/Status/Deck/Song/Filepath",{std::int32_t(0),fixture.u8string()}});
        result=collect(registry,request,[](const RefreshResult &r){return r.snapshot&&r.snapshot->metadata&&r.snapshot->metadata->artwork.status==playing::core::ArtworkStatus::Ready;});
        check(result.snapshot->metadata->title=="Track 1"&&result.snapshot->metadata->artwork.image&&std::filesystem::is_regular_file(std::filesystem::u8path(result.snapshot->metadata->artwork.image->location)));check(registry.snapshot(a.token)==registry.snapshot(b.token));
        client.sendMessage({"/Status/Video/Mixer/Crossfader",{0.f}});
        client.sendMessage({"/Status/Deck/Song/Title",{std::int32_t(0),std::string("Changed without path")}});
        result=collect(registry,request,[](const RefreshResult &r){return r.snapshot&&r.snapshot->metadata&&r.snapshot->metadata->title=="Changed without path";});check(result.snapshot->metadata->artwork.status==playing::core::ArtworkStatus::Missing);
        client.sendMessage({"/Status/Deck/Song/Filepath",{std::int32_t(0),std::string("\\\\server\\private.mp3")}});
        result=collect(registry,request,[](const RefreshResult &r){return r.snapshot&&r.snapshot->metadata;});check(result.snapshot->metadata->artwork.status==playing::core::ArtworkStatus::Missing);
        check(std::filesystem::remove(fixture));
        const auto retained = result.snapshot;
        result = collect(registry, request, [](const RefreshResult &r) { return r.result == Refresh::Unchanged; });
        check(result.snapshot == retained);
        client.sendMessage({"/Status/Video/Mixer/Crossfader", {1.f}});
        result = collect(registry, request, [](const RefreshResult &r) { return r.snapshot && r.snapshot->metadata && r.snapshot->metadata->title == "Track 2"; });
        check(result.snapshot->metadata->artist == u8"Björk");
        for (std::int32_t i = 0; i < 2; ++i) client.sendMessage({"/Status/Deck/Playhead", {i, 0.f, 0.f, 120.f}});
        result = collect(registry, request, [](const RefreshResult &r) { return r.snapshot && !r.snapshot->metadata && r.result == Refresh::Applied; });
        check(result.snapshot->playback == playing::core::Playback::Waiting);
        client.close(); result = collect(registry, request, [](const RefreshResult &r) { return r.result == Refresh::Unavailable; });
        check(result.issue == ProviderIssue::ConnectionLost); check(!result.snapshot->metadata);
        check(registry.release(a.token)); check(port > 0 && registry.activeRequests().size() == 1);
        check(registry.release(b.token)); check(port == 0 && registry.activeRequests().empty()); check(onWorker);
        auto direct = makeSeratoLive(request, options); direct->stop(); direct->start(); direct->start(); direct->stop(); direct->stop(); check(true);
        ProviderRegistry product; check(registerSerato(product) == Registration::Registered);
        check(product.descriptors()[0].automatic && !product.descriptors()[0].history);
        check(product.acquire({"serato", Mode::History}).result == Acquisition::UnsupportedMode);
        check(registerSerato(product) == Registration::Duplicate);
        bool invalid = false; try { makeSeratoLive({"serato", Mode::History}, options); } catch (const std::invalid_argument &) { invalid = true; } check(invalid);
        std::cout << checks << " Serato worker/registry checks; real loopback fixtures, shared lifecycle\n"; return 0;
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
