// SPDX-License-Identifier: MIT
#pragma once
#include "playing/visual/normal-motion.hpp"

namespace playing::visual::normal {
struct TerminalDecoration {
    double cursorOpacity = 1;
    double scanTopPercent = -20;
    double scanOpacity = 0;
};

// Original designs-motion.css opm-cursor/opm-scan. Property tracks are
// independent: the opacity-only 62/86% frames do not interrupt top's ramp.
// Time belongs to the visible host timeline; no clock or resources here.
inline TerminalDecoration terminalDecoration(double milliseconds) noexcept {
    TerminalDecoration result;
    if (!std::isfinite(milliseconds) || milliseconds < 0) return result;
    const double cursor = std::fmod(milliseconds, 4000.0) / 4000.0;
    const double segment = cursor < .5 ? cursor * 2 : (cursor - .5) * 2;
    const double eased = authoredEase(segment, AuthoredEasing::EaseInOut);
    result.cursorOpacity = cursor < .5 ? 1 - .45 * eased : .55 + .45 * eased;
    const double scan = std::fmod(milliseconds, 14000.0) / 14000.0;
    if (scan >= .9) result.scanTopPercent = 100;
    else if (scan > .58) result.scanTopPercent = -20 + 120 * (scan - .58) / .32;
    if (scan > .58 && scan < .62) result.scanOpacity = .035 * (scan - .58) / .04;
    else if (scan >= .62 && scan <= .86) result.scanOpacity = .035;
    else if (scan > .86 && scan < .9) result.scanOpacity = .035 * (.9 - scan) / .04;
    return result;
}
} // namespace playing::visual::normal
