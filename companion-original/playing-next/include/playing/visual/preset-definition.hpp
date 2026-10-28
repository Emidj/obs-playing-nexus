// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "normal-native-layout.hpp"
#include "preset-art.hpp"
#include <QByteArray>
#include <QString>
#include <QFont>
#include <utility>
#include <vector>
namespace playing::visual::normal {
struct ClassicRowDefinition {double left=0,right=0,available=0,gap=0;};
// Ordered affine terms preserve the arithmetic order of the original panels.
// Variables: offset,w,h,s,ly,lw,lh,ty,th,ay,ah. No expression language or I/O.
using ClassicScalar=std::vector<std::pair<std::size_t,double>>;
struct ClassicRectangle {
    std::array<ClassicScalar,4> box;
    QColor color;
    bool accent=false,opacityScaled=false,moving=false;
    double alpha=1;
};
struct ClassicDefinition {
    bool scaled=false,center=false,roundExtents=false,roundQuads=false;
    double top=20,bottom=30,minimumHeight=0,travel=0,period=5500;
    std::array<ClassicRowDefinition,3> rows;
    std::vector<ClassicRectangle> rectangles;
};
struct PresetTextDefinition {
    QString family;
    QColor color;
    double pixelSize=14;
    QFont::Weight weight=QFont::Medium;
    bool italic=false,uppercase=false;
};
struct PresetDefinition {
    QString id,name;
    int setting=0,previewHeight=220;
    bool scalePreviewHeight=true;
    bool nativeRecipe=false,center=false;
    double left=45,available=410,top=26,rowGap=8,bottom=45;
    std::array<PresetTextDefinition,3> text;
    bool scaleClassicText=false,classicOutline=false;
    ClassicDefinition classic;
    QPointF travel{0,-3};
    double period=4500,pulse=.18;
    std::array<ArtLayer,2> art;
};
struct PresetAliasDefinition {QString id,name,target;int setting=0;};
struct PresetCatalogue {
    std::vector<PresetDefinition> presets;
    std::vector<PresetAliasDefinition> aliases;
    const PresetDefinition &operator[](std::size_t index) const {return presets.at(index);}
};
// Bounded, all-or-nothing parser. This checkpoint exposes no file watcher,
// scripts, remote assets, custom imports or mutable render-time catalogue.
PresetCatalogue parsePresetCatalogue(const QByteArray &json);
void validatePresetCompatibility(const PresetCatalogue&,const QByteArray&);
const PresetCatalogue &bundledPresetCatalogue();
const PresetDefinition &presetDefinition(NativePreset preset);
NativeLayout classicLayout(const ClassicDefinition&,int,const std::array<QSize,3>&,int);
oto::visual::Scene classicBackground(const ClassicDefinition&,const NativeLayout&,QColor,double);
NativeLayout redesignedDefinitionLayout(const PresetDefinition&,int,const std::array<QSize,3>&,int);
}
