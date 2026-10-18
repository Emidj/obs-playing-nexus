// SPDX-License-Identifier: GPL-2.0-or-later
#include "playing/visual/authored-text.hpp"
#include <QGuiApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QThread>
#include <set>
#include <thread>
#include <iostream>
using namespace playing::visual::normal;
namespace {unsigned checks=0;void check(bool ok,const char *why){++checks;if(!ok)throw std::runtime_error(why);}
QColor originalColor(const QString &value){
 const auto match=QRegularExpression(QStringLiteral("^rgb\\((\\d+), (\\d+), (\\d+)\\)$")).match(value);
 if(!match.hasMatch())throw std::runtime_error("Unsupported reference color");
 return QColor(match.captured(1).toInt(),match.captured(2).toInt(),match.captured(3).toInt());
}}
int main(int argc,char **argv){QGuiApplication app(argc,argv);try{
 check(argc==2||argc==3,"Original reference path required");QFile input(QString::fromUtf8(argv[1]));check(input.open(QIODevice::ReadOnly),"Reference open");
 const auto cases=QJsonDocument::fromJson(input.readAll()).object()["cases"].toArray();check(cases.size()==60,"All 20 designs and three metadata cases required");
 oto::visual::VisualEngine engine;std::set<QString> presets;QJsonArray measurements;
 for(const auto &value:cases){const auto item=value.toObject();presets.insert(item["preset"].toString());
  const auto rows=item["rows"].toArray();check(rows.size()==3,"All text layers required");
  AuthoredTrackTextStyle style;
  for(int i=0;i<3;++i){const auto row=rows[i].toObject();auto &font=style.rows[i];
   auto families=row["family"].toString().split(',');for(auto &family:families){family=family.trimmed();family.remove('"');family.remove('\'');}
   font.family=families.takeFirst();font.fallbackFamilies=families;
   font.pixelSize=static_cast<int>(row["pixelSize"].toDouble());font.weight=static_cast<QFont::Weight>(row["weight"].toString().toInt());
   font.italic=row["fontStyle"].toString()==QStringLiteral("italic");font.color=originalColor(row["color"].toString());
   const auto spacing=row["spacing"].toString();font.letterSpacing=spacing=="normal"?0:spacing.left(spacing.size()-2).toDouble();
   font.inkPadding=12;style.uppercase[i]=row["transform"].toString()==QStringLiteral("uppercase");
  }
  auto track=std::make_shared<playing::core::TrackState>();track->title=rows[1].toObject()["text"].toString().toUtf8().toStdString();track->artist=rows[2].toObject()["text"].toString().toUtf8().toStdString();
  const auto title=track->title,artist=track->artist;const auto label=rows[0].toObject()["text"].toString();
  const auto prepared=prepareAuthoredTrackText(engine,track,label,style);
  const auto repeated=prepareAuthoredTrackText(engine,track,label,style);
  check(prepared->track==track&&track->title==title&&track->artist==artist,"Display casing mutated or replaced original Track State");
  for(int i=0;i<3;++i){const auto &row=prepared->rows[i];check(row.advance>0&&!row.tiles.empty(),"Original text failed shaping");
   const auto source=rows[i].toObject()["text"].toString();
   const auto independent=prepareAuthoredText(engine,style.uppercase[i]?source.toUpper():source,style.rows[i]);
   check(row.advance==independent.advance,"Track layer differs from direct shaped text");
   check(row.tiles.size()==repeated->rows[i].tiles.size(),"Repeated preparation changed tiles");
   bool ink=false;const auto &image=row.tiles.front().pixels->image;
   for(int y=0;y<image.height()&&!ink;++y)for(int x=0;x<image.width()&&!ink;++x)ink=image.pixelColor(x,y).alpha()>0;
   check(ink,"Original text layer produced an empty raster");
   for(std::size_t j=0;j<row.tiles.size();++j){const auto &tile=row.tiles[j];
    check(tile.pixels==repeated->rows[i].tiles[j].pixels,"Identical layer did not reuse shared cache");
    check(tile.pixels->encoding==oto::visual::RasterEncoding::Premultiplied,"Authored layer encoding changed");
    check(tile.pixels->image.width()<=2048&&tile.pixels->image.height()<=256,"Unbounded text raster");
   }
   QJsonArray faces;
   if(argc==3)prepareAuthoredText(engine,style.uppercase[i]?source.toUpper():source,style.rows[i],[&](const QList<QGlyphRun>&runs,QSize size,QColor,QPointF){
    for(const auto &run:runs){const auto face=run.rawFont();faces.append(QJsonObject{{"family",face.familyName()},{"styleName",face.styleName()},{"weight",face.weight()},{"style",static_cast<int>(face.style())}});}
    QImage image(size,QImage::Format_RGBA8888_Premultiplied);image.fill(Qt::transparent);return image;
   });
   measurements.append(QJsonObject{{"preset",item["preset"]},{"kind",item["kind"]},{"row",i},{"nativeAdvance",row.advance},{"browserAdvance",rows[i].toObject()["advance"]},{"delta",row.advance-rows[i].toObject()["advance"].toDouble()},{"rasterFaces",faces}});
  }
 }
 check(presets.size()==20&&!presets.count(QStringLiteral("terminal")),"Wrong migration scope");
 AuthoredTrackTextStyle style;bool rejected=false;
 try{prepareAuthoredTrackText(engine,nullptr,QStringLiteral("label"),style);}catch(const std::invalid_argument &){rejected=true;}
 check(rejected,"Missing track generated text");
 auto track=std::make_shared<playing::core::TrackState>();track->title="Jóga";
 auto blank=prepareAuthoredTrackText(engine,track,QString(),style);
 check(blank->rows[0].tiles.front().pixels->image.pixelColor(0,0).alpha()==0&&blank->rows[2].tiles.front().pixels->image.pixelColor(0,0).alpha()==0,"Absent metadata produced visible ink");
 bool wrongThreadRejected=false;std::thread worker([&]{try{prepareAuthoredTrackText(engine,track,QStringLiteral("label"),style);}catch(const std::logic_error &){wrongThreadRejected=true;}});worker.join();
 check(wrongThreadRejected,"Track text preparation crossed GUI owner");
 if(argc==3){QFile output(QString::fromUtf8(argv[2]));check(output.open(QIODevice::WriteOnly),"Measurements output open");const auto bytes=QJsonDocument(measurements).toJson();check(output.write(bytes)==bytes.size(),"Measurements complete");}
 std::cout<<"PASS "<<checks<<" checks: 20 designs / 180 native text layers; scene and pixel parity separate\n";return 0;
}catch(const std::exception &error){std::cerr<<"FAIL "<<checks<<": "<<error.what()<<'\n';return 1;}}
