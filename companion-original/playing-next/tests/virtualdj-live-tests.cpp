// SPDX-License-Identifier: MIT
#include "playing/providers/virtualdj-live.hpp"
#include "playing/providers/virtualdj-history.hpp"
#include "virtualdj-fixture.hpp"
#include "../src/providers/media-artwork.hpp"
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
using namespace playing::providers;
class ObservedMemory final : public virtualdj::ProcessMemory {
public:
    ObservedMemory(VirtualDjFixture &fixture, std::atomic<unsigned> &reads, std::atomic<bool> &race)
        : fixture_(fixture), reads_(reads), race_(race), memory_(virtualdj::openReadOnly(GetCurrentProcessId())) {}
    bool alive() const noexcept override { return memory_->alive(); }
    std::wstring imagePath() const override { return memory_->imagePath(); }
    bool read(std::uint64_t address, void *out, std::size_t bytes) override {
        ++reads_;
        if (address == fixture_.base() + 0x4c1fc08 && race_) {
            if (++countReads_ == 2) { fixture_.set<std::uint64_t>(0x4c1fc08, 2); race_ = false; countReads_ = 0; }
        }
        return memory_->read(address, out, bytes);
    }
private:
    VirtualDjFixture &fixture_; std::atomic<unsigned> &reads_; std::atomic<bool> &race_;
    unsigned countReads_ = 0;
    std::unique_ptr<virtualdj::ProcessMemory> memory_;
};
RefreshResult collect(ProviderRegistry &registry, const Request &request) {
    for (unsigned i = 0; i < 500; ++i) {
        auto result = registry.refresh(request);
        if (result.result != Refresh::Pending) return result;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    throw std::runtime_error("worker timeout");
}
int main() {
    unsigned checks = 0;
    auto check = [&checks](bool ok) { if (!ok) throw std::runtime_error("check " + std::to_string(checks + 1)); ++checks; };
    try {
        VirtualDjFixture fixture; std::atomic<unsigned> opens{0}; std::atomic<unsigned> mode{0};
        std::atomic<unsigned> reads{0}; std::atomic<bool> race{false};
        VirtualDjLive::Opener opener = [&] {
            ++opens; virtualdj::Connection connection;
            if (mode == 1) { connection.issue = virtualdj::ProcessIssue::Missing; return connection; }
            if (mode == 2) { connection.issue = virtualdj::ProcessIssue::Multiple; return connection; }
            if (mode == 3) { connection.issue = virtualdj::ProcessIssue::Version; return connection; }
            if (mode == 4) { connection.issue = virtualdj::ProcessIssue::Access; return connection; }
            connection.issue = virtualdj::ProcessIssue::None; connection.module = fixture.base();
            connection.version = virtualdj::verifiedVersion; connection.memory = std::make_unique<ObservedMemory>(fixture, reads, race);
            return connection;
        };
        ProviderRegistry registry; const Request request{"virtualdj"};
        check(registry.registerProvider({"virtualdj", "Fixture", true, false}, [&](const Request &r) {
            return std::make_unique<VirtualDjLive>(r, opener); }) == Registration::Registered);
        check(opens == 0); const auto a = registry.acquire(request), b = registry.acquire(request);
        check(a.result == Acquisition::Acquired && b.result == Acquisition::Acquired); check(opens == 0);
        check(registry.activeRequests().size() == 1); check(registry.consumerCount() == 2);
        check(registry.refresh(request).result == Refresh::Pending);
        auto result = collect(registry, request); check(result.result == Refresh::Applied);
        check(opens == 1); check(result.snapshot->metadata->title == "Track 1");
        check(registry.snapshot(a.token) == registry.snapshot(b.token));
        const auto retained = result.snapshot;
        const auto quiet = reads.load(); std::this_thread::sleep_for(std::chrono::milliseconds(40));
        check(reads == quiet); // No autonomous polling after a completed request.
        race = true; result = collect(registry, request);
        check(result.result == Refresh::TemporaryFailure && result.issue == ProviderIssue::SnapshotChanged);
        check(result.snapshot == retained); fixture.set<std::uint64_t>(0x4c1fc08, 4);
        check(collect(registry, request).result == Refresh::Unchanged);
        check(registry.snapshot(a.token) == retained);
        fixture.master(4); check(collect(registry, request).snapshot->metadata->title == "Track 4");
        fixture.master(0); check(collect(registry, request).snapshot->metadata->title == "Track 4");
        fixture.playing(4, false); check(collect(registry, request).snapshot->metadata->title == "Track 1");
        for (unsigned i = 1; i <= 4; ++i) fixture.playing(i, false);
        result = collect(registry, request); check(!result.snapshot->metadata);
        check(result.snapshot->playback == playing::core::Playback::Waiting);
        fixture.playing(2, true); check(collect(registry, request).snapshot->metadata->title == "Track 2");
        fixture.set<unsigned char>(0x200c0, 2); result = collect(registry, request);
        check(result.result == Refresh::Unavailable && result.issue == ProviderIssue::InvalidLayout);
        check(!result.snapshot->metadata); fixture.playing(2, true);
        check(collect(registry, request).snapshot->metadata->title == "Track 2"); check(opens == 2);
        check(registry.release(a.token)); check(registry.activeRequests().size() == 1);
        check(registry.release(b.token)); check(registry.activeRequests().empty());
        check(registry.refresh(request).result == Refresh::UnknownSession);
        for (unsigned i = 1; i <= 4; ++i) {
            mode = i; const auto c = registry.acquire(request); check(c.result == Acquisition::Acquired);
            result = collect(registry, request);
            check(result.result == (i == 4 ? Refresh::TemporaryFailure : Refresh::Unavailable));
            check(result.issue == (i == 1 ? ProviderIssue::ProcessMissing : i == 2 ? ProviderIssue::ProcessMultiple
                : i == 3 ? ProviderIssue::UnsupportedVersion : ProviderIssue::ProcessAccess));
            mode = 0; check(collect(registry, request).snapshot->metadata->title == "Track 2"); check(registry.release(c.token));
        }
        VirtualDjLive direct(request, opener); direct.stop(); direct.start(); direct.start(); direct.stop(); direct.stop(); direct.start(); direct.stop(); check(true);
        VirtualDjLive invalid({"virtualdj", Mode::History}, opener);
        bool rejected = false; try { invalid.start(); } catch (const std::invalid_argument &) { rejected = true; }
        check(rejected); invalid.stop();
        {const auto atom=[](const std::string &type,const std::string &body){const auto n=static_cast<unsigned>(body.size()+8);std::string out;for(int shift=24;shift>=0;shift-=8)out+=static_cast<char>((n>>shift)&255);return out+type+body;};
         const std::string image="\xff\xd8\xff\xe0picture";const auto bytes=atom("ftyp","M4A ")+atom("moov",atom("udta",atom("meta",std::string(4,'\0')+atom("ilst",atom("covr",atom("data",std::string(8,'\0')+image))))));
         const auto mediaFixture=std::filesystem::temp_directory_path()/("playing-art-"+std::to_string(GetCurrentProcessId())+".m4a");
         {std::ofstream f(mediaFixture,std::ios::binary);f.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));}
         const auto picture=detail::mediaPicture(mediaFixture);check(std::string(picture.begin(),picture.end())==image);
         {std::ofstream f(mediaFixture,std::ios::binary|std::ios::trunc);f.write(bytes.data(),static_cast<std::streamsize>(bytes.size()-3));}
         check(detail::mediaPicture(mediaFixture).empty());std::filesystem::remove(mediaFixture);}
        {
            const auto root = std::filesystem::temp_directory_path();
            const auto xml = root / ("playing-vdj-library-" + std::to_string(GetCurrentProcessId()) + ".xml");
            const auto other = root / ("playing-vdj-library-other-" + std::to_string(GetCurrentProcessId()) + ".xml");
            const auto write = [](const std::filesystem::path &path, const std::string &body) { std::ofstream(path) << body; };
            playing::core::TrackState track; track.title = "Sabotage (Clean)"; track.artist = "Beastie Boys";
            const std::string song = "<Song FilePath=\"H:\\Music\\Sabotage.mp3\"><Tags Author=\"Beastie Boys\" Title=\"Sabotage\" Remix=\"Clean\"/></Song>";
            write(xml, "<VirtualDJ_Database>" + song + "</VirtualDJ_Database>");
            check(detail::virtualDjMediaPath(track, {xml}) == "H:\\Music\\Sabotage.mp3");
            track.title = "Sabotage";
            check(detail::virtualDjMediaPath(track, {xml}) == "H:\\Music\\Sabotage.mp3");
            track.artist = "Wrong artist";
            check(!detail::virtualDjMediaPath(track, {xml}));
            track.artist = "Beastie Boys";
            write(other, "<VirtualDJ_Database>" + song + "</VirtualDJ_Database>");
            check(detail::virtualDjMediaPath(track, {xml, other}) == "H:\\Music\\Sabotage.mp3");
            write(other, "<VirtualDJ_Database><Song FilePath=\"D:\\Other.mp3\"><Tags Author=\"Beastie Boys\" Title=\"Sabotage\"/></Song></VirtualDJ_Database>");
            check(!detail::virtualDjMediaPath(track, {xml, other}));
            write(xml, "<VirtualDJ_Database>" + song); // Found before truncation is still unsafe.
            check(!detail::virtualDjMediaPath(track, {xml}));
            write(xml, "<!DOCTYPE VirtualDJ_Database [<!ENTITY title 'Sabotage'>]><VirtualDJ_Database><Song FilePath=\"H:\\Music\\Sabotage.mp3\"><Tags Author=\"Beastie Boys\" Title=\"&title;\"/></Song></VirtualDJ_Database>");
            check(!detail::virtualDjMediaPath(track, {xml}));
            track.title = "A & B"; track.artist = "Björk";
            write(xml, u8"<VirtualDJ_Database><Song FilePath=\"H:\\Music\\Björk.mp3\"><Tags Author=\"Björk\" Title=\"A &amp; B\"/></Song></VirtualDJ_Database>");
            check(detail::virtualDjMediaPath(track, {xml}) == u8"H:\\Music\\Björk.mp3");
            std::filesystem::remove(xml); std::filesystem::remove(other);
        }
        ProviderRegistry product;
        check(registerVirtualDj(product) == Registration::Registered);
        check(product.descriptors()[0].automatic && product.descriptors()[0].history);
        check(registerVirtualDjHistory(product) == Registration::Duplicate);
        const auto history = product.acquire({"virtualdj", Mode::History, "Z:\\playing-does-not-exist\\history.txt"});
        check(history.result == Acquisition::Acquired); check(product.release(history.token));
        std::cout << checks << " live worker checks; synthetic layout with real Windows read-only transport\n"; return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
