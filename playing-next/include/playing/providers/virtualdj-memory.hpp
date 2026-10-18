// SPDX-License-Identifier: MIT
#pragma once
#include "virtualdj-selection.hpp"
#include <cstddef>

namespace playing::providers::virtualdj {
// Exact reads only. Implementations may fail; never return partial bytes.
class Memory {
public:
    virtual ~Memory() = default;
    virtual bool read(std::uint64_t address, void *output, std::size_t bytes) = 0;
};
struct Decode {
    SelectionIssue issue = SelectionIssue::None;
    Frame frame;
};
// Audited VirtualDJ 8.5.9295.0 x64 layout; bounded reads, no scans or writes.
Decode decode(Memory &memory, std::uint64_t module, const std::string &version);
} // namespace playing::providers::virtualdj
