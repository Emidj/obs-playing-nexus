// SPDX-License-Identifier: MIT
// Behavioral source: obs-playing/serato-protocol.lua. Protocol provenance and
// MIT notice: THIRD-PARTY-NOTICES.txt (Chris Le, serato-connect reference).
#include "playing/providers/serato-state.hpp"
#include <cmath>
namespace playing::providers::serato {
namespace {
bool timeValid(double value) { return std::isfinite(value) && value >= 0; }
bool levelValid(double value) { return std::isfinite(value) && value >= 0 && value <= 1; }
bool textValid(const std::string &text) {
    if (text.size() > 8192) return false;
    for (const unsigned char c : text)
        if (c <= 8 || c == 11 || c == 12 || (c >= 14 && c <= 31)) return false;
    return true; // Matches Lua ingress; Core additionally checks UTF-8/controls.
}
}
void State::clear() noexcept {
    decks_ = {}; faders_ = {}; cross_.reset(); lastStatus_.reset(); selected_.reset();
}
bool State::apply(const Event &event, double now) {
    if (!timeValid(now)) return false;
    if (event.topic == Topic::Crossfader) {
        if (!levelValid(event.values[0])) return false;
        cross_ = event.values[0]; lastStatus_ = now; return true;
    }
    if (event.deck >= decks_.size()) return false;
    if (event.topic == Topic::Upfader) {
        if (!levelValid(event.values[0])) return false;
        faders_[event.deck] = event.values[0]; lastStatus_ = now; return true;
    }
    auto &deck = decks_[event.deck];
    switch (event.topic) {
    case Topic::Title:
    case Topic::Artist: {
        if (!textValid(event.text)) return false;
        auto &field = event.topic == Topic::Title ? deck.title : deck.artist;
        if (field != event.text) { field = event.text; deck.changed = now;
            if(deck.filepath&&now-deck.pathChanged.value_or(0)>.1)deck.filepath.reset();
        }
        break;
    }
    case Topic::Filepath: {
        if(!event.text.empty()&&!core::validImageReference({core::ImageKind::LocalFile,event.text,{}}))return false;
        if(deck.filepath!=event.text){deck.filepath=event.text;deck.pathChanged=now;}
        break;
    }
    case Topic::Valid:
        if (event.values[0] != 0 && event.values[0] != 1) return false;
        deck.valid = event.values[0] == 1;
        if (!*deck.valid) { deck.title.reset(); deck.artist.reset(); deck.filepath.reset();deck.pathChanged.reset();deck.rate = 0; deck.changed = now; }
        break;
    case Topic::Playhead:
        for (const auto value : event.values) if (!std::isfinite(value)) return false;
        deck.rate = event.values[1]; // Position/BPM don't affect selection.
        break;
    default: return false;
    }
    lastStatus_ = now; return true;
}
Selection State::read(double now) {
    if (!timeValid(now)) return {StateIssue::InvalidTime, {}, {}};
    if (!lastStatus_) return {StateIssue::AwaitingTelemetry, {}, {}};
    std::array<double, 2> levels{};
    for (unsigned i = 0; i < levels.size(); ++i) {
        const auto &deck = decks_[i];
        if (deck.valid == false || !deck.title || deck.title->empty() || !deck.artist || !deck.rate
            || std::abs(*deck.rate) <= .001 || now - deck.changed.value_or(now) < .1) continue;
        auto level = faders_[i].value_or(1);
        if (cross_) level *= i == 0 ? 1 - *cross_ : *cross_;
        if (level > .01) levels[i] = level;
    }
    std::optional<unsigned> winner;
    if (selected_ && *selected_ <= 2 && levels[*selected_ - 1] > 0) winner = selected_;
    for (unsigned i = 0; i < levels.size(); ++i)
        if (levels[i] > 0 && (!winner || levels[i] > levels[*winner - 1] + .08)) winner = i + 1;
    if (!winner) {
        selected_.reset(); return {StateIssue::None, {}, core::Observation{core::Playback::Waiting, {}}};
    }
    const auto &deck = decks_[*winner - 1];
    core::Metadata track{*deck.title, *deck.artist};
    if (!core::validMetadata(track)) return {StateIssue::InvalidMetadata, {}, {}};
    selected_ = winner;
    const auto path=now-deck.pathChanged.value_or(now)>=.1?deck.filepath:std::optional<std::string>{};
    return {StateIssue::None, winner, core::Observation{core::Playback::Playing, std::move(track)},path};
}
}
