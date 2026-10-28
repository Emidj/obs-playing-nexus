// SPDX-License-Identifier: GPL-2.0-or-later
#include "playing/visual/normal-gdi-text.hpp"
#include "playing/visual/normal-prepared.hpp"
#include <QGuiApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>
#include <array>
#include <stdexcept>
#include <cmath>
using namespace playing::visual::normal;
int main(int argc,char **argv) {
    QGuiApplication application(argc,argv);unsigned checks=0;
    const auto check=[&checks](bool value,const char *why){++checks;if(!value)throw std::runtime_error(why);};
    try {
        check(argc==2,"Original metrics required");QFile file(QString::fromUtf8(argv[1]));check(file.open(QIODevice::ReadOnly),"Original metrics open");
        const auto cases=QJsonDocument::fromJson(file.readAll()).array();check(cases.size()==18,"All native reference cases");
        for(const auto value:cases) {
            const auto original=value.toObject();const int number=original["name"].toString().section('-',-1).toInt();
            const QString title=number==1?QStringLiteral("Lipstick"):number==2?QString::fromUtf8(u8"Jóga 🎵 日本語"):QStringLiteral("Long native title ")+QStringLiteral("scrolling ").repeated(20);
            const QString artist=number==1?QStringLiteral("Alaia & Gallo"):number==2?QStringLiteral("Björk"):QStringLiteral("Long artist ")+QStringLiteral("overflow ").repeated(12);
            const std::array<QString,3> text{QStringLiteral("NOW PLAYING"),title,artist};
            const std::array<QString,3> keys{QStringLiteral("label"),QStringLiteral("title"),QStringLiteral("artist")};
            const std::array<int,3> heights{14,36,20};
            for(std::size_t i=0;i<text.size();++i) {
                GdiTextStyle style;style.cellHeight=heights[i];style.outline=original["preset"].toString()=="minimal";
                const auto image=rasterGdiText(text[i],style);const auto metrics=original["metrics"].toObject()[keys[i]].toObject();
                check(image.size()==QSize(metrics["width"].toInt(),metrics["height"].toInt()),"Actual OBS GDI metric parity");
                bool ink=false;for(int y=0;y<image.height()&&!ink;++y)for(int x=0;x<image.width()&&!ink;++x)ink=image.pixelColor(x,y).alpha()>0;
                check(ink,"Native GDI raster produced no glyphs");
            }
        }
        oto::visual::VisualEngine engine;
        auto state=std::make_shared<const playing::core::TrackState>(playing::core::TrackState{u8"Jóga 🎵 日本語","Björk"});
        const auto frame=prepareNativeNormal(engine,NativePreset::Prism,state,720);
        const auto again=prepareNativeNormal(engine,NativePreset::Prism,state,720);
        check(frame->track==state&&frame->layout.canvas==QSize(720,166),"Native immutable prepared frame");
        check(frame->text[1].front().pixels->encoding==oto::visual::RasterEncoding::Premultiplied,"Supplementary Unicode must use native fallback, not GDI missing glyphs");
        check(frame->background==again->background,"Shared prepared background identity");
        check(frame->solidRects.size()==6,"Prism retains six original solid quads");
        QImage pixel(2,2,QImage::Format_RGBA8888);pixel.fill(QColor(24,28,36,128));
        const auto straight=engine.prepareSrgbRaster(pixel);
        const auto premul=engine.prepareRaster(pixel.convertToFormat(QImage::Format_RGBA8888_Premultiplied));
        check(straight!=premul&&straight->encoding==oto::visual::RasterEncoding::StraightSrgb
            &&premul->encoding==oto::visual::RasterEncoding::Premultiplied,"Raster encodings have separate cache identities");
        bool rejected=false;try{engine.prepareSrgbRaster(premul->image);}catch(const std::invalid_argument&){rejected=true;}
        check(rejected,"Straight sRGB rejects premultiplied input");
        for(std::size_t i=0;i<frame->text.size();++i)check(frame->text[i].front().pixels==again->text[i].front().pixels,"Shared prepared text identity");
        auto longState=std::make_shared<const playing::core::TrackState>(playing::core::TrackState{std::string(4096,'W'),"Artist"});
        const auto tiled=prepareNativeNormal(engine,NativePreset::Air,longState,400);
        check(tiled->text[1].size()==9,"Original long text retained with bounded bilinear gutters");
        for(const auto &tile:tiled->text[1])check(tile.pixels->image.width()<=2048,"Common engine tile size bounded");
        for(const int width:{400,720,1600})for(const int gap:{0,12,80}) {
            const auto redesign=prepareNativeNormal(engine,NativePreset::Cantilever,longState,width,QStringLiteral("NOW PLAYING"),gap);
            check(redesign->animatedDecoration&&redesign->solidRects.size()==6,"Cantilever native layers and animation");
            check(redesign->layout.canvas.height()<2048,"Redesign canvas bounded");
            for(const auto &rect:redesign->solidRects)check(rect.box.width()==std::floor(rect.box.width())&&rect.box.height()==std::floor(rect.box.height()),"Redesign obeys shared GPU solid extent contract");
            check(redesign->layout.rows[1].overflow>0,"Long title retains scrolling");
            for(const auto &row:redesign->layout.rows)check(QRectF(QPointF{},redesign->layout.canvas).contains(row.box),"Redesign clips inside canvas");
            for(const auto &tile:redesign->text[1])check(tile.pixels->image.width()<=2048,"Redesign glyph tiles bounded");
        }
        std::cout<<"PASS "<<checks<<" native GDI raster/OBS metric checks; pixel parity pending\n";return 0;
    }catch(const std::exception&error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
