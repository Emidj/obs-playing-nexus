// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "normal-native-layout.hpp"
#include <QJsonValue>
#include <QPainter>
#include <vector>
#include <memory>
namespace playing::visual::normal {
struct ArtCondition {std::array<double,8> test{};double threshold=0,below=0,otherwise=0;};
struct ArtScalar {std::array<double,8> terms{};std::shared_ptr<const ArtCondition> condition;}; // offset,h,ty,th,ay,ah,cy,index
struct ArtPoint {ArtScalar x,y;};
struct ArtStop {double at=0;QColor color;};
struct ArtBrush {
    enum class Kind {None,Solid,Linear,Radial};Kind kind=Kind::None;
    QColor color;ArtPoint start,end;ArtScalar radius;std::vector<ArtStop> stops;
};
struct ArtSegment {char kind='M';std::vector<ArtScalar> values;};
struct ArtCommand {
    enum class Kind {Rect,Ellipse,Line,Path,Repeat};Kind kind=Kind::Rect;
    std::array<ArtScalar,4> box;ArtPoint start,end;ArtScalar radius,count;
    ArtBrush fill;QColor stroke;double width=1;int maximum=0;
    std::vector<ArtSegment> segments;std::vector<ArtCommand> children;
};
using ArtLayer=std::vector<ArtCommand>;
std::array<ArtLayer,2> parsePresetArt(const QJsonValue &value);
// CPU preparation only. Immutable commands never execute in the OBS draw path.
void paintPresetArt(QPainter &painter,const ArtLayer &layer,const NativeLayout &layout);
}
