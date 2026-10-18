// SPDX-License-Identifier: GPL-2.0-or-later
#include "playing/visual/authored-text.hpp"
#include <QGuiApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDir>
#include <QPainter>
#include <QRegularExpression>
#include <iostream>
using namespace playing::visual::normal;
int main(int argc,char **argv){
 QGuiApplication app(argc,argv);
 try{
  if(argc<3||argc>6)throw std::runtime_error("Reference and output directory required; optional mode and color raster library");
  if(argc==6&&QString::fromUtf8(argv[5])!=QStringLiteral("color-glyph"))throw std::invalid_argument("Unknown color diagnostic");
  if(argc>=4){const auto mode=QString::fromUtf8(argv[3]);
   if(mode!=QStringLiteral("qt")&&mode!=QStringLiteral("dwrite-downsample")&&mode!=QStringLiteral("dwrite-natural")&&mode!=QStringLiteral("dwrite-gray")&&mode!=QStringLiteral("dwrite-system"))
    throw std::runtime_error("Unknown glyph raster candidate");}
  QFile input(QString::fromUtf8(argv[1]));if(!input.open(QIODevice::ReadOnly))throw std::runtime_error("Reference open failed");
  const auto cases=QJsonDocument::fromJson(input.readAll()).array();if(cases.size()!=12)throw std::runtime_error("All glyph references required");
  const QString folder=QString::fromUtf8(argv[2]);QDir().mkpath(folder);oto::visual::VisualEngine engine;
  for(const auto value:cases){const auto item=value.toObject();AuthoredTextStyle style;
   style.pixelSize=item["pixelSize"].toInt();style.weight=static_cast<QFont::Weight>(item["weight"].toInt());
   style.letterSpacing=item["spacing"].toDouble();style.lineHeight=item["lineHeight"].toDouble();style.baseline=item["baseline"].toDouble();
   style.fallbackFamilies={QStringLiteral("Courier New"),QStringLiteral("Microsoft YaHei"),QStringLiteral("Segoe UI Emoji")};
   style.windowsDownsample=argc==4&&QString::fromUtf8(argv[3])==QStringLiteral("dwrite-downsample");
   style.windowsNatural=argc==4&&QString::fromUtf8(argv[3])==QStringLiteral("dwrite-natural");
   style.windowsGray=argc==4&&QString::fromUtf8(argv[3])==QStringLiteral("dwrite-gray");
   style.windowsSystemPreblend=argc>=4&&QString::fromUtf8(argv[3])==QStringLiteral("dwrite-system");
   if(argc>=5){if(!style.windowsSystemPreblend)throw std::invalid_argument("Color library requires system candidate");style.colorPathLibrary=QString::fromUtf8(argv[4]);}
   style.windowsColorGlyph=argc==6;
   style.windowsRaster=argc==3||style.windowsDownsample||style.windowsNatural||style.windowsGray||style.windowsSystemPreblend;
   const auto rgb=QRegularExpression(QStringLiteral("rgb\\((\\d+), (\\d+), (\\d+)\\)")).match(item["color"].toString());
   if(!rgb.hasMatch())throw std::runtime_error("Invalid original color");style.color=QColor(rgb.captured(1).toInt(),rgb.captured(2).toInt(),rgb.captured(3).toInt());
   const auto prepared=prepareAuthoredText(engine,item["text"].toString(),style);
   if(item["id"].toString()==QStringLiteral("unicode-1"))std::cout<<prepared.resolvedFamilies.join(',').toStdString()<<'\n';
   QImage output(item["width"].toInt(),item["height"].toInt(),QImage::Format_RGBA8888_Premultiplied);output.fill(Qt::transparent);
   {QPainter painter(&output);for(const auto &tile:prepared.tiles){painter.save();painter.setClipRect(QRect(tile.contentX,0,tile.contentWidth,output.height()));painter.drawImage(tile.x,0,tile.pixels->image);painter.restore();}}
   if(!output.save(folder+'/'+item["id"].toString()+QStringLiteral("-native.png")))throw std::runtime_error("Native glyph save failed");
  }
  std::cout<<"PASS 12 native glyph captures; pixel comparison is separate\n";return 0;
 }catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
