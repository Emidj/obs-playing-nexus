// SPDX-License-Identifier: GPL-2.0-or-later
#include "playing/visual/normal-native-layout.hpp"
#include "playing/visual/normal-prepared.hpp"
#include "playing/visual/preset-definition.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace playing::visual::normal {
NativeLayout classicLayout(const ClassicDefinition &definition,int width,const std::array<QSize,3> &metrics,int gap) {
    if(width<400||width>1600||gap<0||gap>80)throw std::invalid_argument("Classic NORMAL layout bounds");
    const auto bounded=[](double value,double maximum){return std::isfinite(value)&&value>=0&&value<=maximum;};
    if(!bounded(definition.top,128)||!bounded(definition.bottom,128)||!bounded(definition.minimumHeight,512))throw std::invalid_argument("Invalid typed classic layout");
    for(const auto &row:definition.rows){
        if(!bounded(row.left,definition.scaled?499:399)||!bounded(row.right,499)||!bounded(row.available,500)||!bounded(row.gap,100))throw std::invalid_argument("Invalid typed classic row");
        if(row.available?row.left+row.available>(definition.scaled?500:400):row.left+row.right>(definition.scaled?498:399))throw std::invalid_argument("Classic row exceeds canvas");
    }
    for(const auto &size:metrics)if(size.width()<0||size.height()<0||size.width()>16384||size.height()>16384)
        throw std::invalid_argument("Invalid native text metrics");
    const double scale=definition.scaled?width/500.0:1.0;
    NativeLayout result;double y=definition.top*scale;
    for(std::size_t i=0;i<metrics.size();++i){
        const auto &row=definition.rows[i];
        if(i)y+=metrics[i-1].height()+gap+row.gap*scale;
        const double available=row.available?row.available*scale:std::max(1.0,width-(row.left+row.right)*scale);
        const double clipped=std::min<double>(metrics[i].width(),available);
        result.rows[i].box={definition.center?(width-clipped)/2.0:row.left*scale,y,clipped,static_cast<double>(metrics[i].height())};
        result.rows[i].overflow=std::max(0,metrics[i].width()-static_cast<int>(available));
    }
    result.canvas={width,static_cast<int>(std::ceil(std::max(definition.minimumHeight*scale,result.rows[2].box.bottom()+definition.bottom*scale)))};
    return result;
}
NativeLayout nativeLayout(NativePreset preset,int width,const std::array<QSize,3> &metrics,int gap) {
    if(redesigned(preset))return redesignedLayout(preset,width,metrics,gap);
    return classicLayout(presetDefinition(preset).classic,width,metrics,gap);
}
oto::visual::Scene classicBackground(const ClassicDefinition &definition,const NativeLayout &layout,QColor accent,double opacity) {
    if(!accent.isValid()||!std::isfinite(opacity)||opacity<0||opacity>1||layout.canvas.width()<400
        ||layout.canvas.width()>1600||layout.canvas.height()<1||layout.canvas.height()>2048)
        throw std::invalid_argument("Invalid NORMAL background");
    const auto &label=layout.rows[0].box,&title=layout.rows[1].box,&artist=layout.rows[2].box;
    const std::array<double,11> values{1.0,static_cast<double>(layout.canvas.width()),static_cast<double>(layout.canvas.height()),layout.canvas.width()/500.0,
        label.y(),label.width(),label.height(),title.y(),title.height(),artist.y(),artist.height()};
    if(definition.rectangles.size()>32)throw std::invalid_argument("Classic rectangle budget");
    oto::visual::Scene scene{layout.canvas,{}};
    for(const auto &rectangle:definition.rectangles){
        std::array<double,4> boxValues{};
        for(std::size_t i=0;i<4;++i){
            const auto &scalar=rectangle.box[i];if(scalar.empty()||scalar.size()>8)throw std::invalid_argument("Classic scalar budget");
            for(const auto &term:scalar){if(term.first>=values.size()||!std::isfinite(term.second)||std::abs(term.second)>2048)throw std::invalid_argument("Invalid classic scalar term");boxValues[i]+=values[term.first]*term.second;}
            if(!std::isfinite(boxValues[i])||std::abs(boxValues[i])>65536)throw std::invalid_argument("Classic evaluated coordinate bounds");
        }
        QRectF box(boxValues[0],boxValues[1],boxValues[2],boxValues[3]);
        if(definition.roundExtents)box.setSize({std::round(box.width()),std::round(box.height())});
        if(box.width()<=0||box.height()<=0)continue;
        QColor color=rectangle.accent?accent:rectangle.color;
        if(!color.isValid()||!std::isfinite(rectangle.alpha)||rectangle.alpha<0||rectangle.alpha>1)throw std::invalid_argument("Invalid classic rectangle color");
        color.setAlphaF(rectangle.alpha*(rectangle.opacityScaled?opacity:1.0));QPainterPath path;path.addRect(box);
        scene.nodes.emplace_back(oto::visual::PathNode{path,QColor(Qt::transparent),color,1});
    }
    return scene;
}
oto::visual::Scene nativeBackground(NativePreset preset,const NativeLayout &layout,QColor accent,double opacity) {
    const auto &definition=presetDefinition(preset);
    if(definition.nativeRecipe)throw std::invalid_argument("Expected classic NORMAL background");
    return classicBackground(definition.classic,layout,accent,opacity);
}
} // namespace playing::visual::normal
