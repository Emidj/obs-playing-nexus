// SPDX-License-Identifier: GPL-2.0-or-later
#include "playing/visual/terminal-prepared.hpp"
namespace playing::visual::normal {
PreparedTerminalTextPtr prepareTerminalText(oto::visual::VisualEngine &engine,TrackPtr track,
    int width,int gap,const QString &label,const AuthoredGlyphRaster &raster) {
    if(!track||!core::validMetadata(*track)||!raster)throw std::invalid_argument("Terminal metadata/backend required");
    // Validate geometry before any raster work; original source range retained.
    terminalLayout(width,gap,{});
    auto result=std::make_shared<PreparedTerminalText>();result->track=std::move(track);
    AuthoredTextStyle labelStyle;labelStyle.pixelSize=11;labelStyle.weight=QFont::Normal;
    labelStyle.lineHeight=13;labelStyle.baseline=10;labelStyle.color=QColor(121,185,149);
    labelStyle.fallbackFamilies={QStringLiteral("Courier New"),QStringLiteral("Microsoft YaHei"),QStringLiteral("Segoe UI Emoji")};
    auto titleStyle=labelStyle;titleStyle.pixelSize=29;titleStyle.weight=QFont::Medium;
    titleStyle.lineHeight=32.46875;titleStyle.baseline=26;titleStyle.color=QColor(163,250,196);
    auto artistStyle=labelStyle;artistStyle.pixelSize=13;artistStyle.letterSpacing=.6;
    artistStyle.lineHeight=18.1875;artistStyle.baseline=13;artistStyle.color=QColor(120,196,152);
    // Original designs.js uppercases only the displayed title and artist.
    result->rows[0]=prepareAuthoredText(engine,label,labelStyle,raster);
    result->rows[1]=prepareAuthoredText(engine,QString::fromUtf8(result->track->title.c_str()).toUpper(),titleStyle,raster);
    result->rows[2]=prepareAuthoredText(engine,QString::fromUtf8(result->track->artist.c_str()).toUpper(),artistStyle,raster);
    result->onAir=prepareAuthoredText(engine,QStringLiteral("[ ON AIR ]"),labelStyle,raster);
    auto promptStyle=labelStyle;promptStyle.pixelSize=30;promptStyle.lineHeight=36;
    promptStyle.baseline=28;promptStyle.color=QColor(89,164,123);
    result->prompt=prepareAuthoredText(engine,QStringLiteral(">"),promptStyle,raster);
    TerminalMetrics metrics;
    for(std::size_t i=0;i<3;++i)metrics.widths[i]=result->rows[i].advance;
    metrics.onAirWidth=result->onAir.advance;metrics.promptWidth=result->prompt.advance;
    result->layout=terminalLayout(width,gap,metrics);
    return result;
}
}
