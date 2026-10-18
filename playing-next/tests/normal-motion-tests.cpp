// SPDX-License-Identifier: MIT
#include "playing/visual/normal-motion.hpp"
#include "playing/visual/terminal-motion.hpp"
#include <iostream>
#include <fstream>
#include <limits>
#include <array>
#include <stdexcept>
using namespace playing::visual::normal;
namespace {
unsigned checks = 0;
void check(bool ok, const char *message) {
    ++checks;
    if (!ok) throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-9; }
}
int main(int argc, char **argv) {
    try {
        if (argc == 2) {
            std::ifstream reference(argv[1]);
            check(reference.good(), "reference file exists");
            char type = 0;
            while (reference >> type) {
                if (type == 'S') {
                    double seconds, distance, speed, pause, expected;
                    check(bool(reference >> seconds >> distance >> speed >> pause >> expected), "scroll reference row");
                    check(near(scrollOffset(seconds, distance, speed, pause), expected), "JavaScript scroll parity");
                } else if (type == 'T') {
                    int kind, incoming, finished;
                    double elapsed, duration, alpha, y, scale;
                    check(bool(reference >> kind >> elapsed >> duration >> alpha >> y >> scale >> incoming >> finished), "transition reference row");
                    check(kind >= 0 && kind <= 2, "transition reference kind");
                    const auto sample = transitionSample(elapsed, duration, static_cast<Transition>(kind));
                    check(near(sample.alpha, alpha) && near(sample.offsetYPercent, y) && near(sample.scale, scale)
                        && sample.incoming == bool(incoming) && sample.finished == bool(finished), "JavaScript transition parity");
                } else if (type == 'A') {
                    double elapsed, duration, delay, expected;
                    unsigned count; int active;
                    check(bool(reference >> elapsed >> duration >> delay >> count), "authored reference timing");
                    check(count >= 2 && count <= 64, "authored reference bounds");
                    std::array<AuthoredKeyframe,64> frames{};
                    for (unsigned i = 0; i < count; ++i) {
                        int easing;
                        check(bool(reference >> frames[i].offset >> easing), "authored keyframe reference");
                        check(easing >= 0 && easing <= 2, "authored easing reference");
                        frames[i].easing = static_cast<AuthoredEasing>(easing);
                    }
                    check(bool(reference >> active >> expected), "browser computed animation reference");
                    const auto sample = authoredMotionSample(elapsed, duration, delay, frames.data(), count);
                    check(sample.active == bool(active), "browser delayed activation parity");
                    if (sample.active) {
                        const double normalized = (sample.from + sample.weight) / (count - 1);
                        check(std::abs(normalized - expected) <= .000002, "browser authored curve parity");
                    }
                } else if (type == 'R') {
                    double elapsed, cursor, top, opacity;
                    check(bool(reference >> elapsed >> cursor >> top >> opacity), "Terminal CSS property row");
                    const auto value = terminalDecoration(elapsed);
                    check(std::abs(value.cursorOpacity - cursor) < .000002, "Terminal cursor CSS parity");
                    check(std::abs(value.scanTopPercent - top) < .0002, "Terminal independent top CSS parity");
                    check(std::abs(value.scanOpacity - opacity) < .000002, "Terminal scan opacity CSS parity");
                } else throw std::runtime_error("Unknown reference row");
            }
            check(reference.eof(), "reference fully consumed");
        }
        // 100 px overflow, 25 px/s, two seconds at each end: 12 s cycle.
        for (double invalid : {-1.0, std::numeric_limits<double>::infinity(),
                               std::numeric_limits<double>::quiet_NaN()}) {
            const auto value = terminalDecoration(invalid);
            check(value.cursorOpacity == 1 && value.scanTopPercent == -20 &&
                  value.scanOpacity == 0, "Terminal invalid time stays at initial state");
        }
        const double times[] = {-2, 0, 1, 2, 3, 4, 6, 7, 8, 9, 10, 12, 15};
        const double offsets[] = {0, 0, 0, 0, 25, 50, 100, 100, 100, 75, 50, 0, 25};
        for (unsigned i = 0; i < sizeof(times) / sizeof(times[0]); ++i)
            check(near(scrollOffset(times[i], 100, 25, 2), offsets[i]), "original scroll timing");
        check(near(scrollOffset(2, 100, 25, 0), 50), "zero pause");
        check(near(scrollOffset(3, 0, 25, 2), 0), "fitting text remains still");
        check(near(scrollOffset(3, 100, 0, 2), 0), "zero speed remains still");
        check(near(scrollOffset(3, 100, 25, -1), 75), "invalid pause bounded");
        check(near(scrollOffset(std::numeric_limits<double>::quiet_NaN(), 100, 25, 2), 0), "NaN bounded");
        const AuthoredKeyframe authored[] = {{0,AuthoredEasing::EaseInOut},{.5,AuthoredEasing::EaseOut},{1,AuthoredEasing::Linear}};
        check(!authoredMotionSample(0,4000,140,authored,3).active, "positive delay remains inactive");
        check(authoredMotionSample(140,4000,140,authored,3).active, "positive delay activation");
        const auto negativeDelay = authoredMotionSample(0,4000,-1000,authored,3);
        check(negativeDelay.active && negativeDelay.from == 0 && near(negativeDelay.weight,.5), "negative delay starts mid-cycle");
        const auto wrapped = authoredMotionSample(4000,4000,0,authored,3);
        check(wrapped.active && wrapped.from == 0 && near(wrapped.weight,0), "infinite decoration wraps at exact boundary");
        check(!authoredMotionSample(0,0,0,authored,3).active, "zero duration rejected");
        check(!authoredMotionSample(0,4000,0,nullptr,3).active, "missing keyframes rejected");
        check(!authoredMotionSample(0,4000,0,authored,65).active, "oversized keyframes rejected before read");
        const AuthoredKeyframe duplicate[] = {{0,AuthoredEasing::Linear},{0,AuthoredEasing::Linear},{1,AuthoredEasing::Linear}};
        check(!authoredMotionSample(0,4000,0,duplicate,3).active, "duplicate offsets rejected");
        // Each family uses the same smoothstep alpha, with the original
        // discontinuous slide sign change at the invisible midpoint.
        for (const auto kind : {Transition::Fade, Transition::Slide, Transition::Zoom}) {
            const auto before = transitionSample(-10, 800, kind);
            const auto out = transitionSample(200, 800, kind);
            const auto swap = transitionSample(400, 800, kind);
            const auto in = transitionSample(600, 800, kind);
            const auto end = transitionSample(800, 800, kind);
            check(near(before.alpha, 1) && !before.incoming, "before start");
            check(near(out.alpha, .5) && !out.incoming, "OUT quarter");
            check(near(swap.alpha, 0) && swap.incoming && !swap.finished, "swap only when invisible");
            check(near(in.alpha, .5) && in.incoming, "IN quarter");
            check(near(end.alpha, 1) && end.finished, "finished");
            if (kind == Transition::Slide) {
                check(near(out.offsetYPercent, -32.5), "slide OUT");
                check(near(swap.offsetYPercent, 65), "slide midpoint");
                check(near(in.offsetYPercent, 32.5), "slide IN");
            } else check(near(out.offsetYPercent, 0), "no slide for other families");
            check(near(swap.scale, kind == Transition::Zoom ? .86 : 1), "zoom minimum");
            check(near(end.scale, 1), "zoom restored");
        }
        check(transitionSample(100, 0, Transition::Fade).incoming, "duration lower bound");
        check(transitionSample(1000, 5000, Transition::Fade).incoming, "duration upper bound");
        std::cout << "PASS " << checks << " NORMAL motion checks (not renderer parity)\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n'; return 1;
    }
}
