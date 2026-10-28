// SPDX-License-Identifier: MIT
#pragma once
#include "playing/core/playing-core.hpp"
#include <array>
#include <cstdint>
#include <string>

namespace playing::providers::virtualdj {
// Selection over an already decoded frame. This is not a process reader.
inline constexpr const char *verifiedVersion = "8.5.9295.0";
struct Deck {
    std::uint64_t address = 0;
    bool playing = false;
    std::optional<core::Metadata> track;
};
struct Frame {
    std::string version;
    unsigned count = 0;
    std::uint64_t master = 0; // Zero means no MASTER.
    bool coherent = false; // Memory reader must recheck fields before setting this.
    std::array<Deck, 4> decks;
};
enum class SelectionIssue { None, UnverifiedVersion, InvalidLayout, InvalidDeck, InvalidMetadata, ChangedSnapshot };
struct Selection {
    SelectionIssue issue = SelectionIssue::None;
    std::optional<unsigned> deck; // One-based, matching the audited Lua.
    std::optional<core::Observation> observation;
};
// Pure bounded decision. Errors never return an observation or commit selection.
Selection select(const Frame &frame, std::optional<unsigned> previous = {});
} // namespace playing::providers::virtualdj
