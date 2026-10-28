// SPDX-License-Identifier: GPL-2.0-or-later
// Isolated diagnostic. No dependency or renderer is installed in the product.
#include <QGuiApplication>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTextLayout>
#include <QGlyphRun>
#include <QLibrary>
#include <QImage>
#include <QDir>
#include <QRegularExpression>
#include <iostream>
#include "include/c/sk_font.h"
#include "include/c/sk_typeface.h"
#include "include/c/sk_surface.h"
#include "include/c/sk_canvas.h"
#include "include/c/sk_paint.h"
#include "include/c/sk_textblob.h"
struct Api {
 QLibrary library;
#define APIS(X) \
 X(sk_fontmgr_create_default) X(sk_fontmgr_unref) X(sk_fontmgr_match_family_style) \
 X(sk_fontstyle_new) X(sk_fontstyle_delete) X(sk_typeface_unref) \
 X(sk_font_new_with_values) X(sk_font_delete) X(sk_font_set_hinting) X(sk_font_set_edging) \
 X(sk_font_set_subpixel) X(sk_font_set_linear_metrics) X(sk_font_set_baseline_snap) \
 X(sk_surface_new_raster_direct) X(sk_surface_get_canvas) X(sk_surface_unref) \
 X(sk_surfaceprops_new) X(sk_surfaceprops_delete) \
 X(sk_textblob_builder_new) X(sk_textblob_builder_delete) X(sk_textblob_builder_alloc_run_pos) \
 X(sk_textblob_builder_make) X(sk_textblob_unref) X(sk_paint_new) X(sk_paint_delete) \
 X(sk_paint_set_color) X(sk_paint_set_antialias) X(sk_canvas_draw_text_blob)
#define MEMBER(name) decltype(&name) name=nullptr;
 APIS(MEMBER)
#undef MEMBER
 explicit Api(const QString &path):library(path){
  if(!QFileInfo(path).isAbsolute())throw std::runtime_error("Absolute pinned native library required");
  QFile binary(path);if(!binary.open(QIODevice::ReadOnly))throw std::runtime_error("Candidate library unavailable");
  QCryptographicHash hash(QCryptographicHash::Sha256);
  if(!hash.addData(&binary)||hash.result().toHex()!=QByteArrayLiteral("5fffeed011731a3255022f003045995f5e96810ca5538c5fe4a91b048b32560e"))
      throw std::runtime_error("Candidate library does not match audited x64 package");
  binary.close();if(!library.load())throw std::runtime_error("Isolated native library load failed");
#define LOAD(name) name=reinterpret_cast<decltype(name)>(library.resolve(#name));if(!name)throw std::runtime_error(#name);
  APIS(LOAD)
#undef LOAD
 }
};
int main(int argc,char **argv){QGuiApplication app(argc,argv);try{
 if(argc!=4&&argc!=6&&argc!=7)throw std::runtime_error("Reference, output and absolute candidate library required; optional hinting/subpixel/lcd");
 const int hinting=argc>=6?QString::fromUtf8(argv[4]).toInt():2;
 const bool subpixel=argc>=6?QString::fromUtf8(argv[5]).toInt()!=0:true;
 const bool lcd=argc==7&&QString::fromUtf8(argv[6])==QStringLiteral("lcd");
 if(argc==7&&!lcd)throw std::runtime_error("Unknown surface diagnostic mode");
 if(hinting<0||hinting>3)throw std::runtime_error("Hinting bounds");
 Api api(QString::fromUtf8(argv[3]));QFile input(QString::fromUtf8(argv[1]));
 if(!input.open(QIODevice::ReadOnly))throw std::runtime_error("Reference unavailable");
 auto manager=api.sk_fontmgr_create_default();if(!manager)throw std::runtime_error("Font manager unavailable");
 QDir().mkpath(QString::fromUtf8(argv[2]));
 for(const auto v:QJsonDocument::fromJson(input.readAll()).array()){
  const auto item=v.toObject();const auto text=item["text"].toString();const int size=item["pixelSize"].toInt();
  const double precision=std::min(1024,32768/size);QFont font;
  font.setFamilies({"Consolas","Courier New","Microsoft YaHei","Segoe UI Emoji"});
  font.setPixelSize(static_cast<int>(size*precision));font.setWeight(static_cast<QFont::Weight>(item["weight"].toInt()));
  font.setHintingPreference(QFont::PreferNoHinting);font.setLetterSpacing(QFont::AbsoluteSpacing,item["spacing"].toDouble()*precision);
  QTextLayout layout(text.isEmpty()?QString(QChar(0xa0)):text,font);layout.beginLayout();auto line=layout.createLine();
  line.setLineWidth(16384*precision);layout.endLayout();
  QImage pixels(item["width"].toInt(),item["height"].toInt(),QImage::Format_RGBA8888_Premultiplied);pixels.fill(Qt::transparent);
  sk_imageinfo_t info{nullptr,pixels.width(),pixels.height(),RGBA_8888_SK_COLORTYPE,PREMUL_SK_ALPHATYPE};
  auto props=lcd?api.sk_surfaceprops_new(0,RGB_H_SK_PIXELGEOMETRY):nullptr;
  auto surface=api.sk_surface_new_raster_direct(&info,pixels.bits(),pixels.bytesPerLine(),nullptr,nullptr,props);
  if(props)api.sk_surfaceprops_delete(props);
  if(!surface)throw std::runtime_error("Raster unavailable");auto paint=api.sk_paint_new();api.sk_paint_set_antialias(paint,true);
  const auto rgb=QRegularExpression("rgb\\((\\d+), (\\d+), (\\d+)\\)").match(item["color"].toString());
  api.sk_paint_set_color(paint,0xff000000u|(rgb.captured(1).toUInt()<<16)|(rgb.captured(2).toUInt()<<8)|rgb.captured(3).toUInt());
  if(!text.isEmpty())for(const auto &run:layout.glyphRuns()){
   auto style=api.sk_fontstyle_new(static_cast<int>(run.rawFont().weight()),5,UPRIGHT_SK_FONT_STYLE_SLANT);
   auto face=api.sk_fontmgr_match_family_style(manager,run.rawFont().familyName().toUtf8().constData(),style);
   api.sk_fontstyle_delete(style);if(!face)throw std::runtime_error("Font face unavailable");
   auto native=api.sk_font_new_with_values(face,static_cast<float>(size),1,0);
   api.sk_font_set_hinting(native,static_cast<sk_font_hinting_t>(hinting));api.sk_font_set_edging(native,lcd?SUBPIXEL_ANTIALIAS_SK_FONT_EDGING:ANTIALIAS_SK_FONT_EDGING);
   api.sk_font_set_subpixel(native,subpixel);api.sk_font_set_linear_metrics(native,true);api.sk_font_set_baseline_snap(native,false);
   auto builder=api.sk_textblob_builder_new();sk_textblob_builder_runbuffer_t buffer{};
   const auto indices=run.glyphIndexes();const auto positions=run.positions();
   api.sk_textblob_builder_alloc_run_pos(builder,native,static_cast<int>(indices.size()),nullptr,&buffer);
   auto glyphs=static_cast<uint16_t*>(buffer.glyphs);auto points=static_cast<float*>(buffer.pos);
   for(qsizetype i=0;i<indices.size();++i){if(indices[i]>65535)throw std::runtime_error("Glyph index bounds");glyphs[i]=static_cast<uint16_t>(indices[i]);
    points[2*i]=static_cast<float>(positions[i].x()/precision);
    points[2*i+1]=static_cast<float>(positions[i].y()/precision+item["baseline"].toDouble()-line.ascent()/precision);}
   auto blob=api.sk_textblob_builder_make(builder);api.sk_canvas_draw_text_blob(api.sk_surface_get_canvas(surface),blob,0,0,paint);
   api.sk_textblob_unref(blob);api.sk_textblob_builder_delete(builder);api.sk_font_delete(native);api.sk_typeface_unref(face);
  }
  api.sk_paint_delete(paint);api.sk_surface_unref(surface);
  if(!pixels.save(QString::fromUtf8(argv[2])+'/'+item["id"].toString()+"-native.png"))throw std::runtime_error("Capture failed");
 }
 api.sk_fontmgr_unref(manager);std::cout<<"12 isolated Skia captures; comparison required\n";return 0;
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
