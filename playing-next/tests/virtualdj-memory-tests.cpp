// SPDX-License-Identifier: MIT
#include "playing/providers/virtualdj-memory.hpp"
#include <map>
#include <iostream>
#include <cstring>
#include <functional>
#include <stdexcept>
using namespace playing::providers::virtualdj;
class Fixture final : public Memory {
public:
    static constexpr std::uint64_t module = 0x100000;
    std::map<std::uint64_t, unsigned char> data;
    std::function<void(std::uint64_t)> hook;
    unsigned reads = 0;
    bool read(std::uint64_t address, void *output, std::size_t bytes) override {
        ++reads;
        if (hook) hook(address);
        auto *out = static_cast<unsigned char *>(output);
        for (std::size_t i = 0; i < bytes; ++i) {
            const auto found = data.find(address + i);
            if (found == data.end()) return false;
            out[i] = found->second;
        }
        return true;
    }
    template<class T> void put(std::uint64_t address, T value) {
        for (std::size_t i = 0; i < sizeof(T); ++i) data[address + i] = static_cast<unsigned char>(value >> (8 * i));
    }
    void text(std::uint64_t address, const std::string &value, std::size_t size = 2048) {
        for (std::size_t i = 0; i < size; ++i) data[address + i] = i < value.size() ? static_cast<unsigned char>(value[i]) : 0;
    }
    void type(std::uint64_t object, std::uint64_t table, const std::string &name) {
        put(object, table); put(table - 8, table + 0x80);
        put<std::uint32_t>(table + 0x80, 1);
        put<std::uint32_t>(table + 0x8c, static_cast<std::uint32_t>(table + 0x100 - module));
        put<std::uint32_t>(table + 0x94, static_cast<std::uint32_t>(table + 0x80 - module));
        text(table + 0x110, name, 160);
    }
    explicit Fixture(unsigned count = 2) {
        put<std::uint64_t>(module + 0x4c1fc08, count); put<std::uint64_t>(module + 0x4c1fbf0, 0x210000);
        for (unsigned i = 0; i < count; ++i) {
            const std::uint64_t offset = i * 0x40000, deck = 0x210000 + offset, song = 0x220000 + offset;
            // Separate deck/song/db/text blocks for each slot.
            const auto d = UINT64_C(0x600000) + i * 0x10000;
            put(module + 0x4ea51f0 + i * 8, deck);
            type(deck, 0x180000, ".?AVCDeck@@");
            put<std::uint32_t>(deck + 0x80, i); put<unsigned char>(deck + 0xc0, 1);
            put(deck + 0x508, song); type(song, 0x181000, ".?AVCSong@@");
            put(song + 0xa0, d); put(d + 0x20, d + 0x1000); put(d + 0x18, d + 0x2000);
            text(d + 0x1000, "Track " + std::to_string(i + 1)); text(d + 0x2000, u8"Björk");
        }
    }
};
int main() {
    unsigned checks = 0;
    auto check = [&checks](bool ok) { if (!ok) throw std::runtime_error("check " + std::to_string(checks + 1)); ++checks; };
    try {
        Fixture f;
        auto d = decode(f, Fixture::module, verifiedVersion);
        check(d.issue == SelectionIssue::None); check(d.frame.coherent);
        check(select(d.frame).deck == 1u); check(d.frame.decks[0].track->artist == u8"Björk");
        Fixture four(4); check(decode(four, Fixture::module, verifiedVersion).issue == SelectionIssue::None);
        const auto before = f.reads; check(decode(f, Fixture::module, "8.5.9296.0").issue == SelectionIssue::UnverifiedVersion); check(f.reads == before);
        check(decode(f, UINT64_MAX, verifiedVersion).issue == SelectionIssue::InvalidLayout);
        Fixture invalid; invalid.put<std::uint64_t>(Fixture::module + 0x4c1fc08, 3);
        check(decode(invalid, Fixture::module, verifiedVersion).issue == SelectionIssue::InvalidLayout);
        Fixture index; index.put<std::uint32_t>(0x210080, 3);
        check(decode(index, Fixture::module, verifiedVersion).issue == SelectionIssue::InvalidDeck);
        Fixture flag; flag.put<unsigned char>(0x2100c0, 2);
        check(decode(flag, Fixture::module, verifiedVersion).issue == SelectionIssue::InvalidDeck);
        Fixture unknown; unknown.type(0x220000, 0x182000, ".?AVUnknown@@");
        check(decode(unknown, Fixture::module, verifiedVersion).issue == SelectionIssue::InvalidDeck);
        Fixture nullDb; nullDb.put<std::uint64_t>(0x2200a0, 0);
        check(decode(nullDb, Fixture::module, verifiedVersion).issue == SelectionIssue::InvalidMetadata);
        nullDb.put<unsigned char>(0x2100c0, 0);
        check(decode(nullDb, Fixture::module, verifiedVersion).issue == SelectionIssue::None);
        Fixture fake; fake.type(0x220000, 0x182000, ".?AVCFakeSong@@");
        check(decode(fake, Fixture::module, verifiedVersion).issue == SelectionIssue::None);
        Fixture emptyArtist; emptyArtist.put<std::uint64_t>(0x600018, 0);
        check(decode(emptyArtist, Fixture::module, verifiedVersion).frame.decks[0].track->artist.empty());
        Fixture huge; huge.text(0x601000, std::string(2048, 'x'));
        check(decode(huge, Fixture::module, verifiedVersion).issue == SelectionIssue::InvalidMetadata);
        Fixture control; control.text(0x601000, "bad\ntext");
        check(decode(control, Fixture::module, verifiedVersion).issue == SelectionIssue::InvalidMetadata);
        Fixture partial; partial.data.erase(0x60103f);
        check(decode(partial, Fixture::module, verifiedVersion).issue == SelectionIssue::InvalidMetadata);
        Fixture race; unsigned hits = 0;
        race.hook = [&race, &hits](std::uint64_t address) { if (address == Fixture::module + 0x4c1fc08 && ++hits == 2) race.put<std::uint64_t>(address, 4); };
        check(decode(race, Fixture::module, verifiedVersion).issue == SelectionIssue::ChangedSnapshot);
        Fixture textRace; unsigned textHits = 0;
        textRace.hook = [&textRace, &textHits](std::uint64_t address) { if (address == 0x601000 && ++textHits == 2) textRace.text(address, "Changed"); };
        check(decode(textRace, Fixture::module, verifiedVersion).issue == SelectionIssue::ChangedSnapshot);
        Fixture master; master.put<std::uint64_t>(Fixture::module + 0x4c1fbf0, 0x999000);
        check(decode(master, Fixture::module, verifiedVersion).issue == SelectionIssue::InvalidDeck);
        std::cout << checks << " VirtualDJ memory checks passed\n"; return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
