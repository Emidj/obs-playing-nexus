// SPDX-License-Identifier: MIT
#pragma once
#include <stdexcept>
namespace playing::visual {
struct PerformanceCadence {int pollingMs,animationMs;};
// Scheduler targets, not measured FPS or hardware resource guarantees.
inline PerformanceCadence performanceCadence(int profile){
    switch(profile){case 0:return {250,33};case 1:return {100,16};case 2:return {50,16};default:throw std::invalid_argument("Unknown performance profile");}
}
}
