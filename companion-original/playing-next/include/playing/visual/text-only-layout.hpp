// SPDX-License-Identifier: MIT
#pragma once
#include "track-visual.hpp"
#include <algorithm>
namespace playing::visual {
inline TrackLayout offsetTextBlock(TrackLayout layout,int x,int y) {
    const auto block=layout.title.box.united(layout.artist.box);
    x=std::clamp(x,-block.left(),layout.canvas.width()-block.right()-1);
    y=std::clamp(y,-block.top(),layout.canvas.height()-block.bottom()-1);
    layout.title.box.translate(x,y);layout.artist.box.translate(x,y);
    return layout;
}
// Geometry only. Reuses the same text/artwork host; no font shaping or I/O.
inline TrackLayout textOnlyLayout(TrackLayout layout,bool vertical,int margin,
        int lineGap,int imageGap,int coverSize) {
    if(layout.normalPreset||layout.canvas.width()<64||layout.canvas.height()<64||
        layout.canvas.width()>2048||layout.canvas.height()>2048||margin<0||margin>128||
        lineGap<0||lineGap>128||imageGap<0||imageGap>128||coverSize<16||coverSize>512)
        throw std::invalid_argument("Invalid text-only layout");
    margin=std::min(margin,(std::min(layout.canvas.width(),layout.canvas.height())-4)/2);
    QRect area=QRect(QPoint{},layout.canvas).adjusted(margin,margin,-margin,-margin);
    if(layout.artworkEnabled) {
        const int maximum=vertical?std::min(area.width(),area.height()/2):std::min(area.height(),area.width()/3);
        const int size=std::max(1,std::min(coverSize,maximum));
        if(vertical) {
            layout.artworkBox=QRect(area.x()+(area.width()-size)/2,area.y(),size,size);
            const int advance=size+std::min(imageGap,area.height()-size-2);
            area.setTop(area.top()+advance);
        } else {
            layout.artworkBox=QRect(area.x(),area.y()+(area.height()-size)/2,size,size);
            const int advance=size+std::min(imageGap,area.width()-size-1);
            area.setLeft(area.left()+advance);
        }
    }
    lineGap=std::min(lineGap,area.height()-2);
    const int titleHeight=std::min(layout.title.pixelSize*3/(layout.title.overflow==oto::visual::Overflow::Wrap?1:2),(area.height()-lineGap)/2);
    const int artistHeight=std::min(layout.artist.pixelSize*3/(layout.artist.overflow==oto::visual::Overflow::Wrap?1:2),area.height()-lineGap-titleHeight);
    const int top=area.y()+((vertical&&layout.artworkEnabled)?0:(area.height()-titleHeight-lineGap-artistHeight)/2);
    layout.title.box=QRect(area.x(),top,area.width(),titleHeight);
    layout.artist.box=QRect(area.x(),top+titleHeight+lineGap,area.width(),artistHeight);
    layout.title.vertical=oto::visual::Vertical::Bottom;
    layout.artist.vertical=oto::visual::Vertical::Top;
    return layout;
}
}
