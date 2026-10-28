// SPDX-License-Identifier: MIT
#pragma once
#include "playing/core/playing-core.hpp"
#include <array>

namespace playing::providers::serato {
// Decoded status events only. No OSC, Bonjour, sockets or playback commands.
enum class Topic { Title, Artist, Valid, Playhead, Upfader, Crossfader, Filepath };
struct Event {
    Topic topic = Topic::Title;
    unsigned deck = 0; // Serato's zero-based index, 0..3; unused for Crossfader.
    std::string text;
    std::array<double, 3> values{}; // OSC floats promoted to double by decoder.
};
enum class StateIssue { None, AwaitingTelemetry, InvalidTime, InvalidMetadata };
struct Selection {
    StateIssue issue = StateIssue::None;
    std::optional<unsigned> deck; // One-based; only 1/2 are selected.
    std::optional<core::Observation> observation;
    std::optional<std::string> mediaPath = std::nullopt; // Local lookup only; never exported as remote artwork.
};
// Port of serato-protocol.lua State. Caller serializes access and supplies
// finite, nonnegative seconds from its clock; no internal clock/timer or I/O.
class State final {
public:
    void clear() noexcept;
    bool apply(const Event &event, double now); // False means ignored/invalid.
    Selection read(double now);
private:
    struct Deck {
        std::optional<std::string> title, artist;
        std::optional<double> rate, changed;
        std::optional<bool> valid;
        std::optional<std::string> filepath;
        std::optional<double> pathChanged;
    };
    std::array<Deck, 4> decks_{};
    std::array<std::optional<double>, 4> faders_{};
    std::optional<double> cross_, lastStatus_;
    std::optional<unsigned> selected_;
};
}
