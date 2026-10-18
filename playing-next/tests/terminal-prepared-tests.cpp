// SPDX-License-Identifier: GPL-2.0-or-later
#include "playing/visual/terminal-prepared.hpp"
#include <QGuiApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QPainter>
#include <iostream>
using namespace playing::visual::normal;
namespace {unsigned checks=0;void check(bool value,const char *why){++checks;if(!value)throw std::runtime_error(why);}}
int main(int argc,char **argv){QGuiApplication app(argc,argv);try{
 check(argc==2,"Original layout reference required");QFile file(QString::fromUtf8(argv[1]));check(file.open(QIODevice::ReadOnly),"Reference open");
 const auto cases=QJsonDocument::fromJson(file.readAll()).array();check(cases.size()==36,"All Terminal layout cases required");
 oto::visual::VisualEngine engine;unsigned calls=0;
 const AuthoredGlyphRaster raster=[&](const QList<QGlyphRun>& runs,QSize size,QColor color,QPointF origin){
  ++calls;QImage image(size,QImage::Format_RGBA8888_Premultiplied);image.fill(Qt::transparent);
  QPainter painter(&image);painter.setPen(color);for(const auto &run:runs)painter.drawGlyphRun(origin,run);return image;
 };
 for(const auto value:cases){const auto item=value.toObject();auto track=std::make_shared<playing::core::TrackState>();
  track->title=item["title"].toString().toUtf8().toStdString();track->artist=item["artist"].toString().toUtf8().toStdString();
  if(track->title.empty()&&track->artist.empty()){
   const auto beforeEmpty=calls;bool rejectedEmpty=false;
   try{prepareTerminalText(engine,track,item["width"].toInt(),item["gap"].toInt(),item["label"].toString(),raster);}catch(const std::invalid_argument &){rejectedEmpty=true;}
   check(rejectedEmpty&&calls==beforeEmpty,"Empty Core metadata generated Terminal work");continue;
  }
  const auto originalTitle=track->title,originalArtist=track->artist;
  const auto prepared=prepareTerminalText(engine,track,item["width"].toInt(),item["gap"].toInt(),item["label"].toString(),raster);
  check(track->title==originalTitle&&track->artist==originalArtist,"Display casing mutated Track State");
  const auto rows=item["rows"].toArray();
  for(int i=0;i<3;++i){check(std::abs(prepared->rows[i].advance-rows[i].toObject()["full"].toDouble())<=1.0/64,"Original text advance differs");
   check(prepared->rows[i].baseline==(i==0?10:i==1?26:13),"Authored baseline differs");}
  const auto rect=[&](QRectF actual,const QJsonArray &expected){
   check(expected.size()==4,"Original rectangle unavailable");
   const std::array<double,4> values{actual.x(),actual.y(),actual.width(),actual.height()};
   for(int i=0;i<4;++i)check(std::abs(values[i]-expected[i].toDouble())<=1.0/32,"Prepared Terminal geometry differs from original");
  };
  rect(prepared->layout.panel,item["panel"].toArray());rect(prepared->layout.header,item["top"].toArray());
  rect(prepared->layout.main,item["main"].toArray());rect(prepared->layout.prompt,item["prompt"].toArray());
  rect(prepared->layout.cursor,item["cursor"].toArray());rect(prepared->layout.bottom,item["bottom"].toArray());
  check(std::abs(prepared->onAir.advance-60.484375)<=1.0/64&&std::abs(prepared->prompt.advance-16.5)<=1.0/64,"Fixed text geometry differs");
  const auto repeated=prepareTerminalText(engine,track,item["width"].toInt(),item["gap"].toInt(),item["label"].toString(),raster);
  for(std::size_t i=0;i<3;++i)for(std::size_t j=0;j<prepared->rows[i].tiles.size();++j)
   check(prepared->rows[i].tiles[j].pixels==repeated->rows[i].tiles[j].pixels,"Shared engine did not reuse Terminal text");
 }
 const auto before=calls;bool rejected=false;
 try{prepareTerminalText(engine,std::make_shared<playing::core::TrackState>(),399,12,QStringLiteral("label"),raster);}catch(const std::invalid_argument &){rejected=true;}
 check(rejected&&calls==before,"Invalid geometry performed raster work");
 std::cout<<"PASS "<<checks<<" Terminal five-text preparation/original geometry/cache checks; full-scene pixel gate separate\n";return 0;
 }catch(const std::exception &e){std::cerr<<"FAIL "<<checks<<": "<<e.what()<<'\n';return 1;}}
