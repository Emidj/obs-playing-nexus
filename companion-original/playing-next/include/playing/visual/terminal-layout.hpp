// SPDX-License-Identifier: MIT
#pragma once
#include <QRectF>
#include <QSize>
#include <array>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace playing::visual::normal {
// Font backend supplies authored local CSS metrics, not scaled output pixels.
struct TerminalMetrics {
    std::array<double,3> widths{};
    double labelHeight=13, titleHeight=32.46875, artistHeight=18.1875;
    double onAirWidth=60.484375, promptWidth=16.5, promptHeight=36;
};
struct TerminalLayout {
    QSize canvas;
    double scale=1;
    QRectF panel,header,main,prompt,cursor,bottom;
    std::array<QRectF,3> rows;
    std::array<double,3> overflow{};
    std::array<QRectF,12> bars;
};
// Geometry is in the original 400-unit space. Compose first, scale once.
// CSS layout quantizes lengths to 1/64 unit; scroll uses rounded clientWidth.
inline TerminalLayout terminalLayout(int width,int gap,const TerminalMetrics &metrics) {
    if(width<400||width>1600||gap<0||gap>80)throw std::invalid_argument("Terminal layout bounds");
    for(double value:metrics.widths)if(!std::isfinite(value)||value<0||value>16384)
        throw std::invalid_argument("Terminal text width bounds");
    for(double value:{metrics.labelHeight,metrics.titleHeight,metrics.artistHeight,
                      metrics.onAirWidth,metrics.promptWidth,metrics.promptHeight})
        if(!std::isfinite(value)||value<=0||value>256)
            throw std::invalid_argument("Terminal font metrics bounds");
    const auto css=[](double value){return std::floor(value*64)/64;};
    TerminalLayout result;result.scale=width/400.0;
    // browser-design.lua reserves both label and artist spacing in output
    // pixels after scaling; the unscaled base height alone clips wide gaps.
    result.canvas=QSize(width,static_cast<int>(std::ceil(210*result.scale+2*std::max(0,gap-12))));
    const double extra=(gap-12)/result.scale;
    const double labelMargin=css(std::max(0.0,extra)),artistMargin=css(std::max(0.0,4+extra));
    const double headerHeight=metrics.labelHeight+labelMargin+9;
    const double mainY=34+headerHeight+10,copyX=35+metrics.promptWidth+9;
    const double copyWidth=365-copyX;
    if(copyWidth<=12)throw std::invalid_argument("Terminal prompt leaves no text viewport");
    const double mainHeight=std::max(metrics.promptHeight,metrics.titleHeight+artistMargin+metrics.artistHeight);
    const double copyY=mainY+css((mainHeight-(metrics.titleHeight+artistMargin+metrics.artistHeight))/2);
    result.header={35,34,330,headerHeight};result.main={35,mainY,330,mainHeight};
    result.prompt={35,mainY+css((mainHeight-metrics.promptHeight)/2),metrics.promptWidth,metrics.promptHeight};
    result.rows[0]={35,34,330-8-metrics.onAirWidth,metrics.labelHeight};
    result.rows[1]={copyX,copyY,std::min(metrics.widths[1],copyWidth-12),metrics.titleHeight};
    result.rows[2]={copyX,copyY+metrics.titleHeight+artistMargin,copyWidth,metrics.artistHeight};
    // Consolas title baseline is 26 and cursor baseline alignment adds 4.
    // This is an original-font contract; custom fonts need their own baseline.
    result.cursor={result.rows[1].right()+4,copyY+4,8,22};
    result.bottom={35,mainY+mainHeight+12,330,3};
    result.panel={20,20,360,result.bottom.bottom()+14-20};
    for(std::size_t i=0;i<3;++i)
        result.overflow[i]=std::max(0.0,metrics.widths[i]-std::floor(result.rows[i].width()+.5));
    // Original fixed 330-unit band: Chromium distributes its four remainder
    // subpixels into bars 1/3/5/7. Preserve that authored material geometry.
    double barX=35;
    for(std::size_t i=0;i<12;++i){
        const double barWidth=(1525+((i<8&&i%2)?1:0))/64.0;
        result.bars[i]={barX,result.bottom.y(),barWidth,3};
        barX+=barWidth+4;
    }
    return result;
}
} // namespace playing::visual::normal
