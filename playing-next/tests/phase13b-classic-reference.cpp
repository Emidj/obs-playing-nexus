// SPDX-License-Identifier: GPL-2.0-or-later
// Frozen text preparation; geometry/background now use the independent 13-C1 fixture.
#include "playing/visual/normal-prepared.hpp"
#include "playing/visual/normal-gdi-text.hpp"
#include "playing/visual/authored-text.hpp"
#include <QFontMetricsF>
#include <QPainter>
#include <QGuiApplication>
#include <QThread>
#include <cmath>
namespace playing::visual::normal {
NativeLayout phase13c1Layout(NativePreset,int,const std::array<QSize,3>&,int);
oto::visual::Scene phase13c1Background(NativePreset,const NativeLayout&,QColor,double);
PreparedNormalPtr preparePhase13bClassic(oto::visual::VisualEngine &engine,NativePreset preset,
    TrackPtr track,int width,const QString &label,int gap) {
    if(!qobject_cast<QGuiApplication*>(QCoreApplication::instance())||QThread::currentThread()!=qApp->thread())
        throw std::logic_error("Native NORMAL preparation requires GUI owner");
    if(!track||!core::validMetadata(*track))throw std::invalid_argument("Invalid native NORMAL track");
    if(redesigned(preset))return prepareRedesignedNormal(engine,preset,std::move(track),width,label,gap);
    if((preset!=NativePreset::Prism&&preset!=NativePreset::Air&&preset!=NativePreset::Cantilever)||width<400||width>1600||gap<0||gap>80)
        throw std::invalid_argument("Invalid native NORMAL preset/width");
    std::array<QString,3> text{label,QString::fromUtf8(track->title.c_str()),QString::fromUtf8(track->artist.c_str())};
    for(std::size_t i=1;i<text.size();++i)if(text[i].isEmpty())text[i]=QString(QChar(0xa0));
    const bool cantilever=preset==NativePreset::Cantilever;
    const std::array<int,3> heights=cantilever?std::array<int,3>{static_cast<int>(18*width/500.0),static_cast<int>(54*width/500.0),static_cast<int>(22*width/500.0)}:std::array<int,3>{14,36,20};
    const std::array<QColor,3> colors=cantilever?std::array<QColor,3>{QColor(39,28,25),QColor(24,41,52),Qt::white}:std::array<QColor,3>{preset==NativePreset::Prism?QColor(24,28,36):QColor(182,228,143),Qt::white,QColor(201,210,220)};
    std::array<QImage,3> rasters;std::array<QSize,3> sizes;
    std::array<PreparedAuthoredText,3> fallbackRows;std::array<bool,3> fallback{};
    for(std::size_t i=0;i<text.size();++i) {
        GdiTextStyle style;style.cellHeight=heights[i];style.outline=preset==NativePreset::Air;style.color=colors[i];
        if(cantilever)style.bold=i!=2;
        rasters[i]=rasterGdiText(text[i],style);sizes[i]=rasters[i].size();
        // GDI+ has no color-font fallback for supplementary Unicode. Retain
        // its line box while preparing these rows with the existing Qt backend.
        for(const auto character:text[i])if(character.isHighSurrogate())fallback[i]=true;
        if(fallback[i]) {
            AuthoredTextStyle font;font.family=style.family;font.pixelSize=std::clamp(static_cast<int>(std::round(style.cellHeight*.75)),8,128);
            font.weight=style.bold?QFont::Bold:QFont::Normal;font.color=style.color;font.boundedEllipsis=true;
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
            sizes[i].setWidth(static_cast<int>(std::ceil(fallbackRows[i].advance))+2);
        }
    }
    auto result=std::make_shared<PreparedNormal>();result->track=std::move(track);
    result->layout=phase13c1Layout(preset,width,sizes,gap);
    auto background=phase13c1Background(preset,result->layout,QColor(182,228,143),.92);
    for(const auto &node:background.nodes) {
        const auto &path=std::get<oto::visual::PathNode>(node);
        auto box=path.path.boundingRect();
        if(cantilever)box=QRectF(std::round(box.x()),std::round(box.y()),std::round(box.width()),std::round(box.height()));
        result->solidRects.push_back({box,path.fill});
    }
    // Accent ornaments move; the readable panels and text keep stable positions.
    // The existing host timeline supplies time, with no private timer or raster work.
    if(cantilever){result->animatedDecoration=true;result->solidRects.back().travel=8*width/500.0;}
    // Reuse the shared premultiplied raster for static panels. Legacy quads
    // multiplied destination alpha twice and produced white fringes in PNG/
    // nested transparent scenes. Keep the moving accent as a separate quad.
    if(cantilever)background.nodes.pop_back();
    result->background=engine.prepare(background);result->usePreparedBackground=true;
    for(std::size_t i=0;i<fallback.size();++i)if(fallback[i])result->text[i]=std::move(fallbackRows[i].tiles);
    // One neighboring pixel on either side preserves bilinear sampling across
    // seams while scrolling. The compositor clips to each non-overlap content.
    for(std::size_t i=0;i<rasters.size();++i)if(!fallback[i])for(int x=0;x<rasters[i].width();x+=2046) {
        const int content=std::min(2046,rasters[i].width()-x),left=std::max(0,x-1);
        const int right=std::min(rasters[i].width(),x+content+1);
        auto tile=rasters[i].copy(left,0,right-left,rasters[i].height()).convertToFormat(QImage::Format_RGBA8888);
        result->text[i].push_back({left,x,content,engine.prepareSrgbRaster(std::move(tile))});
    }
    return result;
}
} // namespace playing::visual::normal
