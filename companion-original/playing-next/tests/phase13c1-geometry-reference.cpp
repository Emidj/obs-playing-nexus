// SPDX-License-Identifier: MIT
#include "playing/visual/normal-native-layout.hpp"
#include "playing/visual/normal-prepared.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace playing::visual::normal {
NativeLayout phase13c1Layout(NativePreset preset,int width,const std::array<QSize,3> &metrics,int gap) {
    if(redesigned(preset))return redesignedLayout(preset,width,metrics,gap);
    if(preset!=NativePreset::Prism&&preset!=NativePreset::Air&&preset!=NativePreset::Cantilever)throw std::invalid_argument("Unknown native NORMAL preset");
    if(width<400||width>1600||gap<0||gap>80)throw std::invalid_argument("Original NORMAL layout bounds");
    for(const auto &size:metrics)if(size.width()<0||size.height()<0||size.width()>16384||size.height()>16384)
        throw std::invalid_argument("Invalid native text metrics");
    if(preset==NativePreset::Cantilever) {
        const double scale=width/500.0;
        NativeLayout result;
        const std::array<double,3> left{115,48,105},available{230,385,350};
        double y=28*scale;
        for(std::size_t i=0;i<metrics.size();++i) {
            if(i)y+=metrics[i-1].height()+gap+16*scale;
            result.rows[i].box={left[i]*scale,y,std::min<double>(metrics[i].width(),available[i]*scale),static_cast<double>(metrics[i].height())};
            result.rows[i].overflow=std::max(0,metrics[i].width()-static_cast<int>(available[i]*scale));
        }
        result.canvas={width,static_cast<int>(std::ceil(result.rows[2].box.bottom()+30*scale))};
        return result;
    }
    const bool air=preset==NativePreset::Air;
    const int x=air?24:42,right=air?24:38,chrome=air?10:16;
    NativeLayout result;
    for(std::size_t i=0;i<metrics.size();++i) {
        const int left=(!air&&i==2)?x+16:x;
        const int available=std::max(1,width-left-right);
        const int clipped=std::min(metrics[i].width(),available);
        result.rows[i].box=QRectF(air?(width-clipped)/2.0:left,0,clipped,metrics[i].height());
        result.rows[i].overflow=std::max(0,metrics[i].width()-available);
    }
    result.rows[0].box.moveTop(20);
    result.rows[1].box.moveTop(20+metrics[0].height()+gap+chrome);
    result.rows[2].box.moveTop(result.rows[1].box.y()+metrics[1].height()+gap);
    result.canvas=QSize(width,static_cast<int>(std::ceil(std::max(air?152.0:166.0,result.rows[2].box.bottom()+30))));
    return result;
}
oto::visual::Scene phase13c1Background(NativePreset preset,const NativeLayout &layout,QColor accent,double opacity) {
    if(preset!=NativePreset::Prism&&preset!=NativePreset::Air&&preset!=NativePreset::Cantilever)throw std::invalid_argument("Unknown native NORMAL preset");
    if(!accent.isValid()||!std::isfinite(opacity)||opacity<0||opacity>1||layout.canvas.width()<400
        ||layout.canvas.width()>1600||layout.canvas.height()<1||layout.canvas.height()>2048)
        throw std::invalid_argument("Invalid NORMAL background");
    oto::visual::Scene scene{layout.canvas,{}};
    const auto rectangle=[&scene,preset](QRectF box,QColor color,double alpha) {
        // Shared solid quads permit fractional positions, but integer extents.
        if(preset==NativePreset::Cantilever)box.setSize({std::round(box.width()),std::round(box.height())});
        if(box.width()<=0||box.height()<=0)return;
        color.setAlphaF(alpha);QPainterPath path;path.addRect(box);
        scene.nodes.emplace_back(oto::visual::PathNode{path,QColor(Qt::transparent),color,1});
    };
    const double w=layout.canvas.width(),h=layout.canvas.height();
    const auto &label=layout.rows[0].box,&title=layout.rows[1].box,&artist=layout.rows[2].box;
    if(preset==NativePreset::Cantilever) {
        const double s=w/500;
        rectangle({32*s,title.y()-10*s,w-59*s,title.height()+24*s},QColor(17,28,39),opacity);
        rectangle({23*s,title.y()-18*s,w-59*s,title.height()+24*s},QColor(241,240,232),opacity);
        rectangle({91*s,label.y()-10*s,280*s,label.height()+20*s},QColor(255,91,64),opacity);
        rectangle({85*s,artist.y()-8*s,w-108*s,artist.height()+16*s},QColor(28,40,48),opacity);
        rectangle({28*s,title.y()-18*s,5*s,title.height()+24*s},QColor(255,91,64),1);
        rectangle({w-56*s,artist.y()+artist.height()+14*s,24*s,3*s},QColor(255,91,64),1);
    } else if(preset==NativePreset::Prism) {
        rectangle({18,title.y()-8,w-18,h-title.y()+8},accent,.20*opacity);
        rectangle({0,title.y()-16,w-18,title.height()+32},QColor(29,37,48),opacity);
        rectangle({0,title.y()-16,6,title.height()+32},accent,1);
        rectangle({24,label.y()-8,label.width()+36,label.height()+16},accent,opacity);
        rectangle({38,artist.y()-4,w-76,artist.height()+12},QColor(42,51,66),opacity);
        rectangle({w-68,h-7,38,3},accent,1);
    } else {
        rectangle({w/2-22,4,44,2},accent,.95);
        const double y=artist.y()+artist.height()+15;
        rectangle({w/2-40,y,27,1},accent,.7);
        rectangle({w/2-3,y-2,6,5},accent,1);
        rectangle({w/2+13,y,27,1},accent,.7);
    }
    return scene;
}
} // namespace playing::visual::normal