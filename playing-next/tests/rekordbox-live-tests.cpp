// SPDX-License-Identifier: MIT
#include "playing/providers/rekordbox-live.hpp"
#include "playing/providers/rekordbox-history.hpp"
#include "rekordbox-memory-fixture.hpp"
#include <iostream>
#include <thread>
#include <chrono>
using namespace playing::providers;
RefreshResult collect(ProviderRegistry &registry, const Request &request) {
    for (unsigned i = 0; i < 1000; ++i) { auto r = registry.refresh(request); if (r.result != Refresh::Pending) return r; std::this_thread::sleep_for(std::chrono::milliseconds(2)); }
    throw std::runtime_error("rekordbox worker timeout");
}
int main() {
    unsigned checks = 0; auto check = [&](bool ok) { if (!ok) throw std::runtime_error("check " + std::to_string(checks + 1)); ++checks; };
    try {
        for (const bool legacy : {false, true}) {
            RekordboxFixture fixture(24, legacy); std::atomic<unsigned> opens{0}, mode{0}; std::atomic<double> clock{1};
            const auto owner = std::this_thread::get_id(); std::atomic<bool> worker{true};
            rekordbox::Opener opener = [&] {
                ++opens; if (std::this_thread::get_id() == owner) worker = false;
                virtualdj::Connection connection;
                if (mode == 1) { connection.issue = virtualdj::ProcessIssue::Missing; return connection; }
                if (mode == 2) { connection.issue = virtualdj::ProcessIssue::Multiple; return connection; }
                if (mode == 3) { connection.issue = virtualdj::ProcessIssue::Access; return connection; }
                if (mode == 4) throw std::runtime_error("fixture opener failure");
                connection.issue = virtualdj::ProcessIssue::None; connection.module = fixture.base();
                connection.version = legacy ? "6.8.7.0" : "7.2.19.0"; connection.memory = virtualdj::openReadOnly(GetCurrentProcessId()); return connection;
            };
            ProviderRegistry registry; const Request request{"rekordbox"};
            check(registry.registerProvider({"rekordbox", "Fixture", true, false}, [&](const Request &r) { return rekordbox::makeLive(r, opener, [&] { return clock.load(); }); }) == Registration::Registered);
            const auto a = registry.acquire(request), b = registry.acquire(request);
            check(a.result == Acquisition::Acquired && b.result == Acquisition::Acquired); check(opens == 0);
            check(collect(registry, request).snapshot->playback == playing::core::Playback::Waiting); check(opens == 1 && worker);
            fixture.put<std::int32_t>(fixture.positions[0], 20); clock = 1.25; check(!collect(registry, request).snapshot->metadata);
            fixture.put<std::int32_t>(fixture.positions[0], 40); clock = 1.5;
            auto result = collect(registry, request); check(result.snapshot->metadata->title == u8"Canción 1");
            check(registry.snapshot(a.token) == registry.snapshot(b.token)); const auto retained = result.snapshot;
            check(collect(registry, request).snapshot == retained); // Same-clock refresh can't fabricate motion.
            clock = 1.75; check(collect(registry, request).snapshot == retained);
            clock = 2.25; check(!collect(registry, request).snapshot->metadata);
            fixture.put<std::int32_t>(fixture.positions[1], 20); clock = 2.5; collect(registry, request);
            fixture.put<std::int32_t>(fixture.positions[1], 40); clock = 2.75; fixture.master(2);
            result = collect(registry, request); check(result.snapshot->metadata && result.snapshot->metadata->title == u8"Canción 2");
            fixture.master(1);
            if (legacy) fixture.put<unsigned char>(fixture.masters[1] + 8, 1); else fixture.put<std::uint64_t>(fixture.masters[1], 1);
            result = collect(registry, request); check(result.result == Refresh::TemporaryFailure && result.issue == ProviderIssue::AmbiguousSelection);
            check(result.snapshot->metadata->title == u8"Canción 2"); fixture.master(2);
            check(registry.release(a.token)); check(registry.activeRequests().size() == 1);
            check(registry.release(b.token)); check(registry.activeRequests().empty()); check(worker);
            for (unsigned i = 1; i <= 4; ++i) {
                mode = i; const auto c = registry.acquire(request); result = collect(registry, request);
                check(result.result == (i <= 2 ? Refresh::Unavailable : Refresh::TemporaryFailure));
                check(result.issue == (i == 1 ? ProviderIssue::ProcessMissing : i == 2 ? ProviderIssue::ProcessMultiple : i == 3 ? ProviderIssue::ProcessAccess : ProviderIssue::ReadError));
                mode = 0; check(collect(registry, request).snapshot->playback == playing::core::Playback::Waiting); check(registry.release(c.token));
            }
        }
        ProviderRegistry product; check(rekordbox::registerProvider(product, "obs-playing-db.dll") == Registration::Invalid);
        check(rekordbox::registerProvider(product, "C:\\playing-fixture-only\\obs-playing-db.dll") == Registration::Registered);
        check(product.descriptors()[0].automatic && product.descriptors()[0].history);
        check(registerRekordboxHistory(product, "C:\\playing-fixture-only\\obs-playing-db.dll") == Registration::Duplicate);
        check(product.acquire({"rekordbox", Mode::History, "C:\\playing-fixture-only\\master.db"}).result == Acquisition::Acquired); product.clear();
        bool invalid = false; try { rekordbox::makeLive({"rekordbox", Mode::History}); } catch (const std::invalid_argument &) { invalid = true; } check(invalid);
        std::cout << checks << " rekordbox shared automatic checks; actual RPM over isolated 6/7 fixture memory\n"; return 0;
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
