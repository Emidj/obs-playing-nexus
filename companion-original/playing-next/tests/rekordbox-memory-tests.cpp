// SPDX-License-Identifier: MIT
#include "playing/providers/rekordbox-live.hpp"
#include "rekordbox-memory-fixture.hpp"
#include <iostream>
#include <limits>
using namespace playing::providers::rekordbox;
Read ready(MemoryReader &reader, double now, std::optional<unsigned> manual = {}) {
    const std::atomic<bool> stop{false};
    for (unsigned i = 0; i < 1000; ++i) { auto result = reader.read(now, stop, manual); if (result.issue != ReadIssue::Searching) return result; }
    throw std::runtime_error("discovery timeout");
}
int main() {
    unsigned checks = 0; auto check = [&](bool ok) { if (!ok) throw std::runtime_error("check " + std::to_string(checks + 1)); ++checks; };
    try {
        check(acceptsVersion("6.8.7.0")); check(acceptsVersion("7.2.19.0")); check(!acceptsVersion("8.0.0.0")); check(!acceptsVersion("7.2.19")); check(!acceptsVersion("7..19.0"));
        check(parseLabels(u8"Título: New: Mix\nArtista: Björk\nÁlbum: Disco")->title == "New: Mix");
        check(parseLabels("Title: Song\r\nArtist: \r\nAlbum: ")->artist.empty());
        check(!parseLabels("Title:bad: Song\nArtist: A\nAlbum: B")); check(!parseLabels("Title: \nArtist: A\nAlbum: B"));
        check(!parseLabels(std::string("Title: \xff\nArtist: A\nAlbum: B"))); check(!parseLabels(std::string(2049, 'x')));
        for (const bool legacy : {false, true}) for (const unsigned shift : {0u, 8u, 24u}) {
            RekordboxFixture fixture(shift, legacy); MemoryReader reader(fixture, fixture.base(), legacy ? "6.8.7.0" : "7.2.19.0");
            for (unsigned i = 1; i <= 4; ++i) { auto result = ready(reader, 1, i); check(result.observation && result.observation->metadata->title == u8"Canción " + std::to_string(i)); fixture.master(i); check(ready(reader, 1, 0).deck == i); }
            fixture.master(1); check(!ready(reader, 1).deck);
            fixture.put<std::int32_t>(fixture.positions[0], 20); check(!ready(reader, 1.25).deck);
            fixture.put<std::int32_t>(fixture.positions[0], 40); check(ready(reader, 1.5).deck == 1u);
            check(ready(reader, 1.75).deck == 1u); check(!ready(reader, 2.25).deck);
            fixture.put<std::int32_t>(fixture.positions[0], 100000); check(!ready(reader, 2.5).deck); // Seek cannot admit song.
            fixture.master(0); check(ready(reader, 3, 1).deck == 1u); // Manual remains independent of MASTER.
            fixture.text(fixture.texts[1], ""); check(!ready(reader, 3, 2).observation->metadata);
        }
        RekordboxFixture fixture; MemoryReader reader(fixture, fixture.base(), "7.2.18.0"); check(ready(reader, 1, 0).deck == 1u);
        unsigned hits = 0; fixture.hook = [&](std::uint64_t address) { if (address == fixture.base() + fixture.masters[0] && ++hits == 2) fixture.put<std::uint64_t>(fixture.masters[0], 0); };
        const std::atomic<bool> stop{false}; check(reader.read(2, stop, 0).issue == ReadIssue::ChangedSnapshot);
        fixture.hook = {}; fixture.master(1); fixture.put<std::uint64_t>(fixture.masters[1], 1); check(reader.read(2, stop).issue == ReadIssue::AmbiguousMaster);
        check(reader.read(std::numeric_limits<double>::quiet_NaN(), stop).issue == ReadIssue::Unavailable);
        MemoryReader bad(fixture, fixture.base(), "8.0.0.0"); check(bad.read(1, stop).issue == ReadIssue::Version);
        const std::atomic<bool> cancelled{true}; check(reader.read(1, cancelled).issue == ReadIssue::Searching);
        std::cout << checks << " rekordbox dynamic memory checks; shifted 6/7 fixtures, no new DJ verification\n"; return 0;
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
