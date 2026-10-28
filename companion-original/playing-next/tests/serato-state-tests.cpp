// SPDX-License-Identifier: MIT
#include "playing/providers/serato-state.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace playing::providers::serato;
int main() {
    unsigned checks = 0;
    auto check = [&checks](bool ok) { if (!ok) throw std::runtime_error("check " + std::to_string(checks + 1)); ++checks; };
    try {
        State state;
        auto event = [&](Topic topic, unsigned deck, double value, double now = 1) {
            Event e; e.topic = topic; e.deck = deck; e.values[0] = value;
            if (topic == Topic::Playhead) { e.values = {10, value, 120}; }
            return state.apply(e, now);
        };
        auto text = [&](Topic topic, unsigned deck, const std::string &value, double now = 1) {
            Event e; e.topic = topic; e.deck = deck; e.text = value; return state.apply(e, now);
        };
        auto waiting = [](const Selection &s) { return s.observation && s.observation->playback == playing::core::Playback::Waiting && !s.observation->metadata; };
        check(state.read(0).issue == StateIssue::AwaitingTelemetry); check(!state.read(0).observation);
        for (unsigned i = 0; i < 2; ++i) {
            check(event(Topic::Valid, i, 1));
            check(text(Topic::Title, i, i ? "Sanctuary" : "Insane"));
            check(text(Topic::Artist, i, i ? "Emma Clair" : "Shermanology"));
            check(event(Topic::Playhead, i, 0)); check(event(Topic::Upfader, i, 1));
        }
        check(waiting(state.read(2))); event(Topic::Playhead, 1, 1);
        check(state.read(2).deck == 2u); check(state.read(2).observation->metadata->title == "Sanctuary");
        event(Topic::Playhead, 0, 1); event(Topic::Crossfader, 0, 0); check(state.read(2).deck == 1u);
        event(Topic::Crossfader, 0, 1); check(state.read(2).deck == 2u);
        event(Topic::Upfader, 1, 0); check(waiting(state.read(2)));
        event(Topic::Crossfader, 0, .5); check(state.read(2).deck == 1u);
        event(Topic::Upfader, 1, 1); check(state.read(2).deck == 1u);
        event(Topic::Crossfader, 0, .53); check(state.read(2).deck == 1u);
        event(Topic::Crossfader, 0, .8); check(state.read(2).deck == 2u);
        text(Topic::Title, 1, "New title", 3); check(state.read(3).deck != 2u);
        text(Topic::Artist, 1, "New artist", 3); check(state.read(3.2).observation->metadata->artist == "New artist");
        event(Topic::Valid, 1, 0, 3); check(state.read(4).deck == 1u);
        event(Topic::Playhead, 0, 0, 3); check(waiting(state.read(4)));
        check(!event(Topic::Crossfader, 0, std::numeric_limits<double>::quiet_NaN()));
        check(!event(Topic::Crossfader, 0, 2)); check(!event(Topic::Upfader, 0, -1));
        check(!event(Topic::Valid, 0, .5)); check(!event(Topic::Playhead, 0, std::numeric_limits<double>::infinity()));
        check(!event(Topic::Playhead, 4, 1)); check(!text(Topic::Title, 0, std::string("a\0b", 3)));
        check(!text(Topic::Artist, 0, std::string(8193, 'x'))); check(!event(Topic::Valid, 0, 1, -1));
        check(state.read(std::numeric_limits<double>::infinity()).issue == StateIssue::InvalidTime);
        state.clear(); check(state.read(5).issue == StateIssue::AwaitingTelemetry);
        // Four-deck telemetry must never invent a four-deck audible mapping.
        for (unsigned i = 2; i < 4; ++i) { text(Topic::Title, i, "Uncertified"); text(Topic::Artist, i, "Artist"); event(Topic::Playhead, i, 1); }
        check(waiting(state.read(2)));
        state.clear(); text(Topic::Title, 0, u8"Jóga 🎵"); event(Topic::Playhead, 0, -1);
        check(waiting(state.read(2))); // Artist must be received, even if empty.
        text(Topic::Artist, 0, ""); check(state.read(2).deck == 1u);
        check(text(Topic::Filepath,0,u8"C:/Música/one.mp3",2));check(state.read(2.2).mediaPath==u8"C:/Música/one.mp3");
        check(text(Topic::Title,0,"Changed without path",3));check(!state.read(3.2).mediaPath);
        check(text(Topic::Filepath,0,"C:/Music/two.mp3",4));check(text(Topic::Title,0,"Path before title",4.01));check(text(Topic::Artist,0,"Artist",4.02));check(state.read(4.2).mediaPath=="C:/Music/two.mp3");
        check(!text(Topic::Filepath,0,std::string("C:/bad\0path",11),4.3));check(state.read(4.5).mediaPath=="C:/Music/two.mp3");
        event(Topic::Valid,0,0,5);check(!state.read(5.2).mediaPath);
        state.clear();text(Topic::Title,0,u8"Jóga 🎵");text(Topic::Artist,0,"");event(Topic::Playhead,0,-1);
        check(state.read(2).observation->metadata->title == u8"Jóga 🎵");
        check(state.read(2).observation->metadata->artist.empty());
        event(Topic::Playhead, 0, .001); check(waiting(state.read(2)));
        event(Topic::Playhead, 0, .002); event(Topic::Upfader, 0, .01); check(waiting(state.read(2)));
        event(Topic::Upfader, 0, .02); check(state.read(2).deck == 1u);
        text(Topic::Title, 0, u8"Jóga 🎵", 10); check(state.read(10).deck == 1u); // Repeated metadata does not reset settle.
        text(Topic::Title, 0, "changed", 10); check(waiting(state.read(10.05))); check(state.read(10.2).deck == 1u);
        text(Topic::Title, 0, std::string(4096, 'x'), 11); check(state.read(12).issue == StateIssue::None);
        text(Topic::Title, 0, std::string(4097, 'x'), 12); check(state.read(13).issue == StateIssue::InvalidMetadata);
        check(!state.read(13).observation);
        text(Topic::Title, 0, "bad\ntext", 13); check(state.read(14).issue == StateIssue::InvalidMetadata);
        text(Topic::Title, 0, std::string("\xff", 1), 14); check(state.read(15).issue == StateIssue::InvalidMetadata);
        text(Topic::Title, 0, "", 15); check(waiting(state.read(16)));
        state.clear(); event(Topic::Crossfader, 0, .5); check(waiting(state.read(20))); // Matches Lua status-only case.
        std::cout << checks << " Serato state checks; decoded fixtures only, no transport/provider\n"; return 0;
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
