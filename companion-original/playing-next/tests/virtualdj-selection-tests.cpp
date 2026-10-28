// SPDX-License-Identifier: MIT
#include "playing/providers/virtualdj-selection.hpp"
#include <iostream>
#include <stdexcept>
using namespace playing::providers::virtualdj;
namespace {
int checks = 0;
void require(bool ok, const char *why) { ++checks; if (!ok) throw std::runtime_error(why); }
Frame frame(unsigned count)
{
    Frame result; result.version = verifiedVersion; result.count = count; result.coherent = true;
    for (unsigned i = 0; i < count; ++i)
        result.decks[i] = {0x210000ULL + i * 0x2000ULL, false,
            playing::core::Metadata{"Track " + std::to_string(i + 1), "Artist"}};
    result.master = result.decks[0].address;
    return result;
}
}
int main()
{
    try {
        auto f = frame(2);
        auto idle = select(f);
        require(!idle.deck && idle.observation->playback == playing::core::Playback::Waiting,
            "loaded paused decks never publish a song");
        f.decks[1].playing = true;
        require(select(f).deck == 2u, "playing deck beats paused MASTER");
        f.decks[0].track->title = "New load";
        require(select(f, 2).deck == 2u, "loading paused deck does not replace active title");
        f.decks[0].playing = true;
        require(select(f, 2).deck == 1u, "active MASTER overrides previous");
        f.master = f.decks[1].address;
        require(select(f, 1).deck == 2u, "MASTER change selects next deck");
        f.master = 0;
        require(select(f, 2).deck == 2u && select(f).deck == 1u, "no MASTER retains active selection then first active");
        f.decks[1].playing = false;
        require(select(f, 2).deck == 1u, "paused selection falls back");
        f.decks[0].playing = false;
        require(!select(f, 1).deck && !select(f, 1).observation->metadata, "all paused clears metadata");
        f.decks[0].playing = true; f.decks[0].track = playing::core::Metadata{u8"Canción 東京 🎵", ""};
        require(select(f).observation->metadata->title == u8"Canción 東京 🎵"
            && select(f).observation->metadata->artist.empty(), "Unicode and absent artist");
        f.coherent = false;
        require(select(f).issue == SelectionIssue::ChangedSnapshot && !select(f).observation,
            "torn snapshot cannot publish");
        f.coherent = true; f.version = "8.5.9296.0";
        require(select(f).issue == SelectionIssue::UnverifiedVersion, "unverified patch rejected");
        f.version = verifiedVersion; f.count = 3;
        require(select(f).issue == SelectionIssue::InvalidLayout, "unverified deck count rejected");
        f = frame(4); f.decks[2].track.reset(); f.decks[3].track.reset();
        require(!select(f).deck, "four-deck empty internal slots idle");
        f.decks[1].playing = true;
        require(select(f).deck == 2u, "empty internal slots do not block active deck");
        f.decks[2].track = playing::core::Metadata{"Third", ""}; f.decks[2].playing = true;
        f.master = f.decks[2].address;
        require(select(f, 2).deck == 3u, "deck 3 supported");
        f.decks[3].track = playing::core::Metadata{"Fourth", ""}; f.decks[3].playing = true;
        f.master = f.decks[3].address;
        require(select(f, 3).deck == 4u, "deck 4 supported");
        f.master = 0xdeadbeef;
        require(select(f).issue == SelectionIssue::InvalidDeck, "unknown MASTER rejected");
        f.master = 0; f.decks[3].address = f.decks[2].address;
        require(select(f).issue == SelectionIssue::InvalidDeck, "duplicate deck identity rejected");
        f = frame(2); f.decks[1].track->title = std::string(2048, 'x');
        require(select(f).issue == SelectionIssue::InvalidMetadata, "oversized paused metadata rejected");
        f.decks[1].track->title = std::string(1, static_cast<char>(0xff));
        require(select(f).issue == SelectionIssue::InvalidMetadata, "invalid UTF8 rejected");
        std::cout << "PASS: " << checks << " VirtualDJ selection checks (decoded fixtures, no process reader)\n";
        return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
