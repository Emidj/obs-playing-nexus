// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "normal-native-layout.hpp"
#include "normal-timeline.hpp"
#include "visual/visual-engine.hpp"
#include <optional>
#include "text-effects.hpp"
namespace playing::visual::normal {
struct PresetDefinition;
struct TextOverride {
    std::optional<QString> family;
    std::optional<int> pixelSize;
    std::optional<bool> bold,italic;
    std::optional<QColor> color;
    TextEffects effects;
    bool operator==(const TextOverride& other)const{return family==other.family&&pixelSize==other.pixelSize&&bold==other.bold&&italic==other.italic&&color==other.color&&effects==other.effects;}
};
using TextOverrides=std::array<TextOverride,2>; // Title and artist; label unchanged.
PresetDefinition customizedDefinition(const PresetDefinition&,const TextOverrides&);
struct TextTile {int x=0,contentX=0,contentWidth=0;oto::visual::FramePtr pixels;};
struct SolidRect {QRectF box;QColor color;double travel=0,period=5500;};
struct PaintLayer {
    oto::visual::FramePtr pixels;
    QPointF position,travel;
    double period=5500,pulse=0;
};
struct PreparedNormal {
    TrackPtr track;
    NativeLayout layout;
    oto::visual::FramePtr background;
    std::vector<SolidRect> solidRects;
    std::array<std::vector<TextTile>,3> text;
    bool animatedDecoration=false;
    bool legacyTextBlend=false,usePreparedBackground=false;
    std::vector<PaintLayer> ornaments;
};
using PreparedNormalPtr=std::shared_ptr<const PreparedNormal>;
// Uses the host's existing engine, never a renderer/cache per preset. GUI only.
PreparedNormalPtr prepareNativeNormal(oto::visual::VisualEngine &engine,NativePreset preset,
    TrackPtr track,int width,const QString &label=QStringLiteral("NOW PLAYING"),int gap=12,const TextOverrides& overrides={});
NativeLayout redesignedLayout(NativePreset preset,int width,const std::array<QSize,3>&metrics,int gap);
PreparedNormalPtr prepareRedesignedNormal(oto::visual::VisualEngine &engine,NativePreset preset,
    TrackPtr track,int width,const QString &label,int gap,const TextOverrides& overrides={});
// GUI preparation from a validated immutable definition; no publication/reload.
PreparedNormalPtr prepareRedesignedDefinition(oto::visual::VisualEngine&,const PresetDefinition&,
    TrackPtr,int,const QString&,int,const TextOverrides& overrides={});
} // namespace playing::visual::normal
