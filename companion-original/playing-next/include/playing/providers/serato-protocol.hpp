// SPDX-License-Identifier: MIT
#pragma once
#include "serato-state.hpp"
#include <variant>
#include <vector>
namespace playing::providers::serato {
inline constexpr std::size_t maxFrame = 524288;
using Blob = std::vector<unsigned char>;
using Argument = std::variant<std::int32_t, float, std::string, Blob>;
struct Message { std::string path; std::vector<Argument> arguments; };
struct Decoded { bool valid = false; std::vector<Message> messages; };
Decoded decode(const std::string &packet); // No sentinel; errors publish no messages.
std::string encode(const Message &message); // Includes sentinel; throws on invalid input.
struct Split { bool valid = false; std::string rest; std::vector<std::string> frames; };
Split split(const std::string &buffer, const std::string &incoming);
bool applyStatus(State &state, const Message &message, double now);
}
