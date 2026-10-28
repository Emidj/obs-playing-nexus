// SPDX-License-Identifier: MIT
#include "playing/providers/virtualdj-selection.hpp"

namespace playing::providers::virtualdj {
namespace {
bool pointer(std::uint64_t address) noexcept
{ return address >= 65536 && address < 140737488355328ULL && address % 8 == 0; }
Selection fail(SelectionIssue issue) { return {issue, {}, {}}; }
}
Selection select(const Frame &frame, std::optional<unsigned> previous)
{
    if (frame.version != verifiedVersion) return fail(SelectionIssue::UnverifiedVersion);
    if (frame.count != 2 && frame.count != 4) return fail(SelectionIssue::InvalidLayout);
    bool knownMaster = frame.master == 0;
    for (unsigned i = 0; i < frame.count; ++i) {
        const auto &deck = frame.decks[i];
        if (!pointer(deck.address)) return fail(SelectionIssue::InvalidDeck);
        for (unsigned j = 0; j < i; ++j)
            if (frame.decks[j].address == deck.address) return fail(SelectionIssue::InvalidDeck);
        if (deck.address == frame.master) knownMaster = true;
        if (deck.track && (!core::validMetadata(*deck.track) || deck.track->title.size() > 2047
            || deck.track->artist.size() > 2047)) return fail(SelectionIssue::InvalidMetadata);
        // Lua accepts empty fake songs; CSong with null metadata is valid only paused.
        // Missing active metadata must be rejected during memory decoding, before this frame.
    }
    if (!knownMaster) return fail(SelectionIssue::InvalidDeck);
    if (!frame.coherent) return fail(SelectionIssue::ChangedSnapshot);
    std::optional<unsigned> selected;
    for (unsigned i = 0; i < frame.count; ++i)
        if (frame.decks[i].playing && frame.decks[i].track && frame.decks[i].address == frame.master) selected = i + 1;
    if (!selected && previous && *previous >= 1 && *previous <= frame.count) {
        const auto &deck = frame.decks[*previous - 1];
        if (deck.playing && deck.track) selected = previous;
    }
    if (!selected) {
        for (unsigned i = 0; i < frame.count; ++i)
            if (frame.decks[i].playing && frame.decks[i].track) { selected = i + 1; break; }
    }
    if (!selected) return {SelectionIssue::None, {}, core::Observation{core::Playback::Waiting, {}}};
    return {SelectionIssue::None, selected,
        core::Observation{core::Playback::Playing, frame.decks[*selected - 1].track}};
}
} // namespace playing::providers::virtualdj
