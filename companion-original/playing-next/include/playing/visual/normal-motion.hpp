// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cmath>

namespace playing::visual::normal {
enum class AuthoredEasing { Linear, EaseInOut, EaseOut };
struct AuthoredKeyframe { double offset; AuthoredEasing easing; };
struct AuthoredSample {
    unsigned from = 0, to = 0;
    double weight = 0;
    bool active = false;
};

// CSS uses the x coordinate of its cubic curve as time. Solving x first is
// essential: smoothstep and directly evaluating y(time) both change the motion.
inline double authoredEase(double progress, AuthoredEasing easing) noexcept {
    progress = std::clamp(progress, 0.0, 1.0);
    if (easing == AuthoredEasing::Linear || progress == 0 || progress == 1)
        return progress;
    const double x1 = easing == AuthoredEasing::EaseOut ? 0 : .42;
    const double x2 = .58;
    const auto coordinate = [](double t, double a, double b) {
        const double inverse = 1 - t;
        return 3 * inverse * inverse * t * a + 3 * inverse * t * t * b + t * t * t;
    };
    double low = 0, high = 1;
    for (unsigned i = 0; i < 40; ++i) {
        const double mid = (low + high) / 2;
        if (coordinate(mid, x1, x2) < progress) low = mid;
        else high = mid;
    }
    return coordinate((low + high) / 2, 0, 1);
}

// Original NORMAL decorations: infinite, normal direction, no fill, linear
// effect timing with per-keyframe easing. The caller supplies visible time;
// this sampler owns no clock, allocation, timer, renderer or provider.
// Invalid contracts fail closed; property-specific interpolation belongs to
// the prepared scene, including preserving transform order and percentages.
inline AuthoredSample authoredMotionSample(double elapsedMilliseconds,
        double durationMilliseconds, double delayMilliseconds,
        const AuthoredKeyframe *frames, unsigned count) noexcept {
    AuthoredSample sample;
    if (!frames || count < 2 || count > 64 ||
        !std::isfinite(elapsedMilliseconds) || elapsedMilliseconds < 0 ||
        !std::isfinite(durationMilliseconds) || durationMilliseconds <= 0 || durationMilliseconds > 60000 ||
        !std::isfinite(delayMilliseconds) || std::abs(delayMilliseconds) > 60000 ||
        frames[0].offset != 0 || frames[count - 1].offset != 1) return sample;
    for (unsigned i = 0; i < count; ++i) {
        if (!std::isfinite(frames[i].offset) || frames[i].offset < 0 || frames[i].offset > 1 ||
            (i && frames[i].offset <= frames[i - 1].offset) ||
            (frames[i].easing != AuthoredEasing::Linear && frames[i].easing != AuthoredEasing::EaseInOut &&
             frames[i].easing != AuthoredEasing::EaseOut)) return sample;
    }
    const double local = elapsedMilliseconds - delayMilliseconds;
    if (local < 0 || !std::isfinite(local)) return sample;
    const double progress = std::fmod(local, durationMilliseconds) / durationMilliseconds;
    sample.active = true;
    for (unsigned i = 1; i < count; ++i) {
        if (progress < frames[i].offset || i == count - 1) {
            sample.from = i - 1; sample.to = i;
            sample.weight = authoredEase((progress - frames[i - 1].offset) /
                (frames[i].offset - frames[i - 1].offset), frames[i - 1].easing);
            return sample;
        }
    }
    return {};
}
// Port of obs-playing/designs.js offset(). Inputs use the original local CSS
// coordinate system (host divides speed by the preset scale).
inline double scrollOffset(double seconds, double distance, double speed,
                           double pause) noexcept {
    if (!std::isfinite(seconds) || !std::isfinite(distance) ||
        !std::isfinite(speed) || !std::isfinite(pause) ||
        distance <= 0 || speed <= 0) return 0;
    pause = std::max(0.0, pause);
    const double travel = distance / speed;
    const double cycle = 2 * (travel + pause);
    if (!std::isfinite(cycle) || cycle <= 0) return 0;
    const double phase = std::fmod(std::max(0.0, seconds), cycle);
    if (phase < pause) return 0;
    if (phase < pause + travel) return (phase - pause) * speed;
    if (phase < 2 * pause + travel) return distance;
    return std::max(0.0, distance - (phase - 2 * pause - travel) * speed);
}

enum class Transition { Fade, Slide, Zoom };
struct TransitionSample {
    double alpha = 1;
    double offsetYPercent = 0;
    double scale = 1;
    bool incoming = false;
    bool finished = false;
};

// A sample is read-only. The host must swap the latest pending track once at
// incoming=true, keep the label unchanged, and reset scroll after finished.
// No timer, clock, allocation, Core mutation or resource loading happens here.
inline TransitionSample transitionSample(double elapsedMilliseconds,
        double durationMilliseconds, Transition kind) noexcept {
    if (!std::isfinite(elapsedMilliseconds)) elapsedMilliseconds = 0;
    if (!std::isfinite(durationMilliseconds)) durationMilliseconds = 0;
    const double duration = std::clamp(durationMilliseconds, 200.0, 2000.0);
    const double progress = std::clamp(elapsedMilliseconds / duration, 0.0, 1.0);
    TransitionSample result;
    result.incoming = progress >= .5;
    result.finished = progress >= 1;
    const double phase = result.incoming ? (progress - .5) * 2 : progress * 2;
    const double eased = phase * phase * (3 - 2 * phase);
    result.alpha = result.incoming ? eased : 1 - eased;
    if (kind == Transition::Slide)
        result.offsetYPercent = (result.incoming ? 1 - result.alpha : -(1 - result.alpha)) * 65;
    if (kind == Transition::Zoom) result.scale = .86 + .14 * result.alpha;
    return result;
}
} // namespace playing::visual::normal
