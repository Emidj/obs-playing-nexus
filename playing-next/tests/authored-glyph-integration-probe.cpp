// SPDX-License-Identifier: GPL-2.0-or-later
// Preparation integration only. The backend remains outside the OBS module.
#define NOMINMAX
#include <Windows.h>
#include "playing/visual/authored-text.hpp"
#include "playing/visual/native-glyph-raster.hpp"
#include <QGuiApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDir>
#include <QPainter>
#include <QRegularExpression>
#include <iostream>
#include <future>
#include <limits>
using namespace playing::visual::normal;
int main(int argc,char **argv){
 QGuiApplication app(argc,argv);
 try{
  if(argc!=4)throw std::invalid_argument("Reference, new output folder and native color library required");
  QFile input(QString::fromUtf8(argv[1]));if(!input.open(QIODevice::ReadOnly))throw std::runtime_error("Reference open failed");
  const auto cases=QJsonDocument::fromJson(input.readAll()).array();if(cases.size()!=12)throw std::runtime_error("All glyph references required");
  const QString folder=QString::fromUtf8(argv[2]);if(QFileInfo::exists(folder)||!QDir().mkpath(folder))throw std::runtime_error("Require new capture directory");
  const QString binary=QString::fromUtf8(argv[3]);unsigned calls=0;QList<QGlyphRun> colorRuns;
  const auto moduleName=QFileInfo(binary).fileName().toStdWString();
  const bool absentInitially=GetModuleHandleW(moduleName.c_str())==nullptr;
  auto native=std::make_unique<NativeGlyphRasterizer>(binary);
  if(native->colorLibraryLoads()!=0)throw std::runtime_error("Native color library eagerly loaded");
  QImage shadowMask(QSize(64,64),QImage::Format_RGBA8888_Premultiplied);shadowMask.fill(Qt::transparent);
  if(native->rasterShadow(shadowMask,QColor(99,233,142,68),7)!=shadowMask||native->colorLibraryLoads()!=0)
   throw std::runtime_error("Transparent shadow performed native loading");
  for(int y=24;y<40;++y)for(int x=24;x<40;++x)shadowMask.setPixelColor(x,y,Qt::white);
  const auto sharpShadow=native->rasterShadow(shadowMask,QColor(99,233,142,68),0);
  if(sharpShadow.pixelColor(32,32).alpha()!=68||native->colorLibraryLoads()!=0)
   throw std::runtime_error("Zero-radius shadow loaded library or changed alpha");
  const AuthoredGlyphRaster raster=[&](const QList<QGlyphRun> &runs,QSize size,QColor color,QPointF origin){
   ++calls;const auto before=native->colorLibraryLoads();auto image=native->raster(runs,size,color,origin);
   if(!before&&native->colorLibraryLoads())colorRuns=runs;return image;
  };
  oto::visual::VisualEngine engine;
  for(const auto value:cases){const auto item=value.toObject();AuthoredTextStyle style;
   style.pixelSize=item["pixelSize"].toInt();style.weight=static_cast<QFont::Weight>(item["weight"].toInt());
   style.letterSpacing=item["spacing"].toDouble();style.lineHeight=item["lineHeight"].toDouble();style.baseline=item["baseline"].toDouble();
   style.fallbackFamilies={QStringLiteral("Courier New"),QStringLiteral("Microsoft YaHei"),QStringLiteral("Segoe UI Emoji")};
   const auto rgb=QRegularExpression(QStringLiteral("rgb\\((\\d+), (\\d+), (\\d+)\\)")).match(item["color"].toString());
   if(!rgb.hasMatch())throw std::runtime_error("Original color invalid");style.color=QColor(rgb.captured(1).toInt(),rgb.captured(2).toInt(),rgb.captured(3).toInt());
   const auto text=item["text"].toString();const auto before=calls;
   const auto prepared=prepareAuthoredText(engine,text,style,raster);
   if(text.isEmpty()&&calls!=before)throw std::runtime_error("Empty text invoked backend");
   const auto repeated=prepareAuthoredText(engine,text,style,raster);
   if(repeated.tiles.size()!=prepared.tiles.size())throw std::runtime_error("Repeated preparation differs");
   for(std::size_t i=0;i<prepared.tiles.size();++i)if(prepared.tiles[i].pixels!=repeated.tiles[i].pixels)
    throw std::runtime_error("Shared engine did not reuse identical native raster");
   QImage output(item["width"].toInt(),item["height"].toInt(),QImage::Format_RGBA8888_Premultiplied);output.fill(Qt::transparent);
   {QPainter painter(&output);for(const auto &tile:prepared.tiles){painter.save();painter.setClipRect(QRect(tile.contentX,0,tile.contentWidth,output.height()));painter.drawImage(tile.x,0,tile.pixels->image);painter.restore();}}
   if(!output.save(folder+'/'+item["id"].toString()+QStringLiteral("-native.png")))throw std::runtime_error("Native glyph save failed");
  }
  if(native->colorLibraryLoads()!=1||!GetModuleHandleW(moduleName.c_str()))throw std::runtime_error("Color library must be shared by every color glyph");
  if(colorRuns.isEmpty())throw std::runtime_error("Color glyph fixture did not exercise native library");
  const auto shadow=native->rasterShadow(shadowMask,QColor(99,233,142,68),7);
  if(shadowMask.pixelColor(32,32)!=QColor(Qt::white)||shadow.pixelColor(32,32).alpha()<60||
     shadow.pixelColor(22,32).alpha()==0||shadow.pixelColor(0,0).alpha()!=0||native->colorLibraryLoads()!=1)
   throw std::runtime_error("Native shadow coverage, padding or shared owner contract failed");
  for(int y=0;y<64;++y)for(int x=0;x<64;++x){const auto *pixel=shadow.constScanLine(y)+x*4;
   if(pixel[3]>68||pixel[0]>pixel[3]||pixel[1]>pixel[3]||pixel[2]>pixel[3]||
      std::abs(int(pixel[3])-shadow.pixelColor(63-x,y).alpha())>1)
    throw std::runtime_error("Native blur broke symmetry or premultiplication");
  }
  unsigned shadowBounds=0;for(double radius:{-1.0,65.0,std::numeric_limits<double>::quiet_NaN()}){
   try{native->rasterShadow(shadowMask,Qt::white,radius);}catch(const std::invalid_argument &){++shadowBounds;}}
  if(shadowBounds!=3)throw std::runtime_error("Shadow radius bounds not enforced");
  if(!shadow.save(folder+QStringLiteral("/shadow-primitive.png")))throw std::runtime_error("Shadow evidence save failed");
  unsigned bounds=0;
  for(int mode=0;mode<3;++mode){try{
   const auto size=mode==0?QSize(2049,33):QSize(64,33);
   const auto origin=mode==1?QPointF(std::numeric_limits<double>::quiet_NaN(),0):QPointF();
   QList<QGlyphRun> runs;if(mode==2)runs.resize(4097);
   native->raster(runs,size,Qt::white,origin);
  }catch(const std::invalid_argument &){++bounds;}}
  if(bounds!=3)throw std::runtime_error("Native raster bounds not enforced");
  auto foreign=std::async(std::launch::async,[&]{try{native->raster({},QSize(32,32),Qt::white,{});return false;}catch(const std::logic_error &){return true;}});
  if(!foreign.get())throw std::runtime_error("Foreign thread accepted by native owner");
  NativeGlyphRasterizer wrong(QFileInfo(QString::fromUtf8(argv[1])).absoluteFilePath());
  bool hashRejected=false;try{wrong.raster(colorRuns,QSize(1024,33),Qt::white,{});}catch(const std::runtime_error &){hashRejected=true;}
  if(!hashRejected||wrong.colorLibraryLoads()!=0)throw std::runtime_error("Untrusted binary hash was accepted or retained");
  AuthoredTextStyle style;unsigned rejected=0;
  for(int mode=0;mode<3;++mode){
   const AuthoredGlyphRaster invalid=[mode](const QList<QGlyphRun> &,QSize size,QColor,QPointF){
    if(mode==0)return QImage();
    if(mode==1)return QImage(size,QImage::Format_RGBA8888);
    throw std::runtime_error("Backend failed");
   };
   try{prepareAuthoredText(engine,QStringLiteral("test"),style,invalid);}catch(const std::runtime_error &){++rejected;}
  }
  if(rejected!=3)throw std::runtime_error("Native backend failures not propagated");
  native.reset();
  if(absentInitially&&GetModuleHandleW(moduleName.c_str()))throw std::runtime_error("Native color module retained after host destruction");
  std::cout<<"PASS 12 shared-engine captures/cache, lazy color library, empty text, 3 failed-backend guards, 3 bounds, foreign owner and binary hash rejection; shadow_premul_symmetry_padding=1; shadow_bounds=3; module_release_checked="<<absentInitially<<"; pixel gate separate\n";return 0;
 }catch(const std::exception &error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
