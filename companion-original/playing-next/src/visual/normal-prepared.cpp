// SPDX-License-Identifier: GPL-2.0-or-later
#include "playing/visual/normal-prepared.hpp"
#include "playing/visual/normal-gdi-text.hpp"
#include "playing/visual/authored-text.hpp"
#include "playing/visual/preset-definition.hpp"
#include <QFontMetricsF>
#include <QPainter>
#include <QGuiApplication>
#include <QThread>
#include <cmath>
namespace playing::visual::normal {
PreparedNormalPtr prepareNativeNormal(oto::visual::VisualEngine &engine,NativePreset preset,
    TrackPtr track,int width,const QString &label,int gap,const TextOverrides &overrides) {
    if(!qobject_cast<QGuiApplication*>(QCoreApplication::instance())||QThread::currentThread()!=qApp->thread())
        throw std::logic_error("Native NORMAL preparation requires GUI owner");
    if(!track||!core::validMetadata(*track))throw std::invalid_argument("Invalid native NORMAL track");
    if(redesigned(preset))return prepareRedesignedNormal(engine,preset,std::move(track),width,label,gap,overrides);
    if(width<400||width>1600||gap<0||gap>80)
        throw std::invalid_argument("Invalid native NORMAL preset/width");
    std::array<QString,3> text{label,QString::fromUtf8(track->title.c_str()),QString::fromUtf8(track->artist.c_str())};
    for(std::size_t i=1;i<text.size();++i)if(text[i].isEmpty())text[i]=QString(QChar(0xa0));
    const auto definition=customizedDefinition(presetDefinition(preset),overrides);
    std::array<QImage,3> rasters;std::array<QSize,3> sizes;
    std::array<PreparedAuthoredText,3> fallbackRows;std::array<bool,3> fallback{};
    for(std::size_t i=0;i<text.size();++i) {
        const auto &row=definition.text[i];
        GdiTextStyle style;style.family=row.family;
        style.cellHeight=static_cast<int>(definition.scaleClassicText?row.pixelSize*width/500.0:row.pixelSize);
        if(i&&overrides[i-1].pixelSize)style.cellHeight=*overrides[i-1].pixelSize;
        style.outline=definition.classicOutline;style.color=row.color;
        style.bold=row.weight==QFont::Bold;style.italic=row.italic;
        if(i){style.underline=overrides[i-1].effects.underline;style.strikeout=overrides[i-1].effects.strikeout;}
        if(style.family.size()>=32){QFont font(style.family);font.setPixelSize(style.cellHeight);font.setBold(style.bold);font.setItalic(style.italic);QFontMetricsF metrics(font);rasters[i]=QImage(2,std::min(256,int(std::ceil(metrics.height()))),QImage::Format_ARGB32);rasters[i].fill(Qt::transparent);fallback[i]=true;}else rasters[i]=rasterGdiText(text[i],style);sizes[i]=rasters[i].size();
        // GDI+ has no color-font fallback for supplementary Unicode. Retain
        // its line box while preparing these rows with the existing Qt backend.
        for(const auto character:text[i])if(character.isHighSurrogate())fallback[i]=true;
        if(fallback[i]) {
            AuthoredTextStyle font;font.family=style.family;font.pixelSize=std::clamp(static_cast<int>(std::round(style.cellHeight*.75)),8,128);
            font.weight=style.bold?QFont::Bold:QFont::Normal;font.color=style.color;font.boundedEllipsis=true;
            font.italic=style.italic;if(i&&overrides[i-1].pixelSize)font.pixelSize=*overrides[i-1].pixelSize;
            if(i)font.effects=overrides[i-1].effects;
            font.fallbackFamilies={QStringLiteral("Microsoft YaHei"),QStringLiteral("Segoe UI Emoji")};
            QFont measure(font.family);measure.setPixelSize(font.pixelSize);measure.setWeight(font.weight);
            font.lineHeight=sizes[i].height();font.baseline=std::min(font.lineHeight,QFontMetricsF(measure).ascent());
            fallbackRows[i]=prepareAuthoredText(engine,text[i],font);
            if(style.outline)for(auto &tile:fallbackRows[i].tiles) {
                const auto &original=tile.pixels->image;
                QImage edge(original.size(),QImage::Format_RGBA8888_Premultiplied);edge.fill(Qt::transparent);
                for(int y=0;y<edge.height();++y)for(int x=0;x<edge.width();++x)edge.scanLine(y)[x*4+3]=static_cast<unsigned char>(original.constScanLine(y)[x*4+3]*191/255);
                QImage outlined(original.size(),QImage::Format_RGBA8888_Premultiplied);outlined.fill(Qt::transparent);
                QPainter painter(&outlined);for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)if(dx||dy)painter.drawImage(dx,dy,edge);
                painter.drawImage(0,0,original);painter.end();tile.pixels=engine.prepareRaster(std::move(outlined));
            }
            sizes[i]=QSize(static_cast<int>(std::ceil(fallbackRows[i].advance))+2,fallbackRows[i].height);
        }
        if(i&&!fallback[i]){const int pad=overrides[i-1].effects.padding();if(pad){QImage padded(rasters[i].size()+QSize(pad*2,pad*2),QImage::Format_ARGB32);padded.fill(Qt::transparent);{QPainter painter(&padded);painter.drawImage(pad,pad,rasters[i]);}rasters[i]=std::move(padded);sizes[i]=rasters[i].size();}}
    }
    auto result=std::make_shared<PreparedNormal>();result->track=std::move(track);
    result->layout=nativeLayout(preset,width,sizes,gap);
    auto background=nativeBackground(preset,result->layout);
    for(const auto &node:background.nodes) {
        const auto &path=std::get<oto::visual::PathNode>(node);
        auto box=path.path.boundingRect();
        if(definition.classic.roundQuads)box=QRectF(std::round(box.x()),std::round(box.y()),std::round(box.width()),std::round(box.height()));
        result->solidRects.push_back({box,path.fill});
    }
    // Accent ornaments move; the readable panels and text keep stable positions.
    // The existing host timeline supplies time, with no private timer or raster work.
    if(definition.classic.travel){
        if(result->solidRects.size()!=definition.classic.rectangles.size())throw std::invalid_argument("Moving classic rectangle missing");
        result->animatedDecoration=true;result->solidRects.back().travel=definition.classic.travel*width/500.0;
        result->solidRects.back().period=definition.classic.period;
    }
    // Reuse the shared premultiplied raster for static panels. Legacy quads
    // multiplied destination alpha twice and produced white fringes in PNG/
    // nested transparent scenes. Keep the moving accent as a separate quad.
    if(definition.classic.travel)background.nodes.pop_back();
    result->background=engine.prepare(background);result->usePreparedBackground=true;
    for(std::size_t i=0;i<fallback.size();++i)if(fallback[i])result->text[i]=std::move(fallbackRows[i].tiles);
    // One neighboring pixel on either side preserves bilinear sampling across
    // seams while scrolling. The compositor clips to each non-overlap content.
    for(std::size_t i=0;i<rasters.size();++i)if(!fallback[i]){const TextEffects effect=i?overrides[i-1].effects:TextEffects{};const int gutter=1+effect.padding(),span=2048-2*gutter;for(int x=0;x<rasters[i].width();x+=span) {
        const int content=std::min(span,rasters[i].width()-x),left=std::max(0,x-gutter);
        const int right=std::min(rasters[i].width(),x+content+gutter);
        auto tile=rasters[i].copy(left,0,right-left,rasters[i].height()).convertToFormat(QImage::Format_RGBA8888);
        tile=applyTextEffects(std::move(tile),effect).convertToFormat(QImage::Format_RGBA8888);
        result->text[i].push_back({left,x,content,engine.prepareSrgbRaster(std::move(tile))});
    }}
    return result;
}
} // namespace playing::visual::normal
