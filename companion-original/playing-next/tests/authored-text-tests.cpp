// SPDX-License-Identifier: GPL-2.0-or-later
#include "playing/visual/authored-text.hpp"
#include <QGuiApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTextLayout>
#include <iostream>
using namespace playing::visual::normal;
namespace {unsigned checks=0;void check(bool ok,const char *why){++checks;if(!ok)throw std::runtime_error(why);}}
int main(int argc,char **argv){
 QGuiApplication application(argc,argv);
 try{
    check(argc>=2,"Original Terminal font inputs required");
    QFile file(QString::fromUtf8(argv[1]));check(file.open(QIODevice::ReadOnly),"Font reference open");
    const auto cases=QJsonDocument::fromJson(file.readAll()).array();check(cases.size()==36,"All original font inputs required");
    oto::visual::VisualEngine engine;QJsonArray measurements;
    for(const auto value:cases){const auto original=value.toObject();const auto rows=original["rows"].toArray();
        for(int i=0;i<3;++i){
            AuthoredTextStyle style;style.pixelSize=i==0?11:i==1?29:13;
            style.weight=i==1?QFont::Medium:QFont::Normal;style.letterSpacing=i==2?.6:0;
            const auto text=rows[i].toObject()["text"].toString();
            const auto prepared=prepareAuthoredText(engine,text,style);const auto repeated=prepareAuthoredText(engine,text,style);
            check(prepared.advance>0&&prepared.height>0&&!prepared.tiles.empty(),"Bounded shaped strip prepared");
            check(prepared.tiles.size()==repeated.tiles.size(),"Stable tile count");
            for(std::size_t j=0;j<prepared.tiles.size();++j){const auto &tile=prepared.tiles[j];
                check(tile.pixels==repeated.tiles[j].pixels,"Shared engine reuses identical tile");
                check(tile.pixels->encoding==oto::visual::RasterEncoding::Premultiplied,"Group-compatible premultiplied text");
                check(tile.pixels->image.width()<=2048&&tile.pixels->image.height()<=256,"GPU tile bounded");
                check(tile.contentWidth>0&&tile.contentWidth<=2046,"Non-overlap tile span bounded");
                if(j){const auto &previous=prepared.tiles[j-1];
                    const int overlapStart=tile.x,overlapEnd=previous.x+previous.pixels->image.width();
                    for(int x=overlapStart;x<overlapEnd;++x)for(int y=0;y<prepared.height;++y)
                        check(previous.pixels->image.pixelColor(x-previous.x,y)==tile.pixels->image.pixelColor(x-tile.x,y),"Adjacent tile gutters contain identical glyph pixels");
                }
            }
            const double browser=rows[i].toObject()["full"].toDouble();
            check(std::abs(prepared.advance-browser)<=1.0/64,"Original Terminal advance within one CSS subpixel");
            if(!text.isEmpty()){
                bool ink=false;const auto &image=prepared.tiles.front().pixels->image;
                for(int y=0;y<image.height()&&!ink;++y)for(int x=0;x<image.width()&&!ink;++x)ink=image.pixelColor(x,y).alpha()>0;
                check(ink,"Native text raster contains glyph ink");
            }
            if(argc==4){
                for(int hint=0;hint<5;++hint)for(bool design:{false,true}){
                    const double precision=hint==4?1024:1;
                    QFont font(style.family);font.setPixelSize(static_cast<int>(style.pixelSize*precision));font.setWeight(style.weight);
                    font.setLetterSpacing(QFont::AbsoluteSpacing,style.letterSpacing*precision);
                    font.setHintingPreference(hint==4?QFont::PreferNoHinting:static_cast<QFont::HintingPreference>(hint));
                    QTextLayout probe(text.isEmpty()?QString(QChar(0xa0)):text,font);
                    QTextOption option;option.setWrapMode(QTextOption::NoWrap);option.setUseDesignMetrics(design);
                    probe.setTextOption(option);probe.beginLayout();auto line=probe.createLine();line.setLineWidth(16384*precision);probe.endLayout();
                    measurements.append(QJsonObject{{"case",original["id"]},{"row",i},{"hint",hint},{"design",design},
                        {"nativeAdvance",line.naturalTextWidth()/precision},{"browserAdvance",browser},{"delta",line.naturalTextWidth()/precision-browser}});
                }
            }
            measurements.append(QJsonObject{{"case",original["id"]},{"width",original["width"]},{"gap",original["gap"]},
                {"row",i},{"nativeAdvance",prepared.advance},{"browserAdvance",browser},{"delta",prepared.advance-browser}});
        }
    }
    AuthoredTextStyle style;const auto empty=prepareAuthoredText(engine,QString(),style);
    AuthoredTextStyle slanted;slanted.family=QStringLiteral("Arial");slanted.pixelSize=42;slanted.inkPadding=12;
    const auto upright=prepareAuthoredText(engine,QStringLiteral("Lipstick"),slanted);
    slanted.italic=true;
    const auto italic=prepareAuthoredText(engine,QStringLiteral("Lipstick"),slanted);
    check(upright.tiles.front().pixels->image!=italic.tiles.front().pixels->image,"Italic style reused upright raster");
    const auto italicAgain=prepareAuthoredText(engine,QStringLiteral("Lipstick"),slanted);
    check(italic.tiles.front().pixels==italicAgain.tiles.front().pixels,"Identical italic raster missed shared cache");
    bool nativeItalic=false;
    prepareAuthoredText(engine,QStringLiteral("Lipstick"),slanted,[&](const QList<QGlyphRun>&runs,QSize size,QColor,QPointF){
        for(const auto &run:runs)nativeItalic=nativeItalic||run.rawFont().style()!=QFont::StyleNormal;
        QImage image(size,QImage::Format_RGBA8888_Premultiplied);image.fill(Qt::transparent);return image;
    });
    check(nativeItalic,"Native backend lost italic font face");
    check(empty.tiles.front().pixels->image.pixelColor(0,0).alpha()==0,"Empty text reserves transparent line");
    bool rejected=false;try{prepareAuthoredText(engine,QString(4097,QChar('A')),style);}catch(const std::invalid_argument &){rejected=true;}
    check(rejected,"Oversized UTF-8 input rejected");
    rejected=false;try{prepareAuthoredText(engine,QStringLiteral("First\nSecond"),style);}catch(const std::invalid_argument &){rejected=true;}
    check(rejected,"Unsupported multiline candidate fails explicitly");
    style.pixelSize=128;const auto largest=prepareAuthoredText(engine,QStringLiteral("W"),style);
    check(largest.height<=256&&largest.advance>0,"Maximum font size remains bounded");
    AuthoredTextStyle paddedStyle;paddedStyle.inkPadding=12;
    const auto padded=prepareAuthoredText(engine,QStringLiteral("Track"),paddedStyle);
    paddedStyle.inkPadding=0;const auto unpadded=prepareAuthoredText(engine,QStringLiteral("Track"),paddedStyle);
    check(padded.advance==unpadded.advance&&padded.height==unpadded.height&&padded.baseline==unpadded.baseline,"Ink padding changed logical layout");
    check(padded.tiles.front().x==-12&&padded.tiles.front().contentX==-12&&padded.tiles.front().pixels->image.height()==padded.height+24,"Ink padding origin/extent lost");
    const int localY=12;bool paddedInk=false;
    for(int y=localY;y<localY+unpadded.height&&!paddedInk;++y)for(int x=12;x<padded.tiles.front().pixels->image.width()&&!paddedInk;++x)
        paddedInk=padded.tiles.front().pixels->image.pixelColor(x,y).alpha()>0;
    check(paddedInk,"Padded raster lost text");
    paddedStyle.inkPadding=33;rejected=false;try{prepareAuthoredText(engine,QStringLiteral("Track"),paddedStyle);}catch(const std::invalid_argument &){rejected=true;}
    check(rejected,"Excessive ink padding accepted");
    bool ink=false;const auto &largestImage=largest.tiles.front().pixels->image;
    for(int y=0;y<largestImage.height()&&!ink;++y)for(int x=0;x<largestImage.width()&&!ink;++x)ink=largestImage.pixelColor(x,y).alpha()>0;
    check(ink,"Maximum font size raster contains ink");
    if(argc>=3){QFile output(QString::fromUtf8(argv[2]));check(output.open(QIODevice::WriteOnly),"Measurement output open");
        const auto bytes=QJsonDocument(measurements).toJson();check(output.write(bytes)==bytes.size(),"Measurement output complete");}
    std::cout<<"PASS "<<checks<<" authored text bounds/cache/encoding checks; font deltas are measurements, not parity PASS\n";
    return 0;
 }catch(const std::exception &error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
