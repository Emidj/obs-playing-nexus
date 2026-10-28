// SPDX-License-Identifier: MIT
#pragma once
#include "visual/scene.hpp"
#include <array>
#include <stdexcept>
#include <string_view>
#include <string>
#include <vector>
namespace playing::visual::normal {
enum class NativePreset { Prism, Air, Cantilever, Oblique, Aura, Orbit, Terminal, Rupture, Dune, Chrome, Monolith, Portal, Beacon, Laser, Matrix, Lotus, Tide, Silk, Obsidian, Lens, Bubblegum, Tuning, Trace };
struct PresetEntry {std::string id,name;NativePreset preset;int setting=0;};
// Named enum tokens remain stable for compatibility; metadata and dispatch are
// resolved from the immutable bundled catalogue, including additional handles.
const std::vector<PresetEntry> &normalCatalogue();
const std::vector<PresetEntry> &normalAliases();
NativePreset presetFromSetting(int value);
NativePreset presetFromId(std::string_view id);
bool redesigned(NativePreset value);
int normalPreviewHeight(int setting,int width);
// Order matches the original renderer: label, title, artist. Metrics come from
// the font backend; this contract performs no shaping, I/O or clock access.
struct NativeRow { QRectF box; int overflow = 0; };
struct NativeLayout { QSize canvas; std::array<NativeRow,3> rows; };
NativeLayout nativeLayout(NativePreset preset,int width,
    const std::array<QSize,3> &metrics,int gap=12);
oto::visual::Scene nativeBackground(NativePreset preset,const NativeLayout &layout,
    QColor accent=QColor(182,228,143),double opacity=.92);
} // namespace playing::visual::normal
