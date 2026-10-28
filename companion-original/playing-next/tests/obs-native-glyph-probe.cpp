// SPDX-License-Identifier: GPL-2.0-or-later
// Actual GPU composition; fixtures are test-only and never enter the product.
#include "playing/visual/authored-text.hpp"
#include "playing/visual/native-glyph-raster.hpp"
#include "obs-visual-renderer.hpp"
#include <QGuiApplication>
#include <QMetaObject>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QPainter>
#include <atomic>
#include <cstdio>
namespace {
std::atomic<int> status{-2};char evidence[1024]{};
void run() noexcept {
 try {
  using namespace playing::visual::normal;
  const QString root=QString::fromUtf8(PLAYING_GLYPH_FIXTURE_ROOT);
  const QString output=qEnvironmentVariable("PLAYING_GLYPH_GPU_OUTPUT");
  if(output.isEmpty()||QFileInfo::exists(output)||!QDir().mkpath(output))throw std::runtime_error("New output directory required");
  const QString reference=qEnvironmentVariable("PLAYING_GLYPH_REFERENCE_JSON",root+"/outputs/phase12l-terminal-glyphs/reference.json");
  QFile input(reference);
  if(!input.open(QIODevice::ReadOnly))throw std::runtime_error("Reference unavailable");
  const auto cases=QJsonDocument::fromJson(input.readAll()).array();
  if(cases.size()<12||cases.size()>32)throw std::runtime_error("Incomplete reference");
  NativeGlyphRasterizer native(root+"/work/phase12l-skia-candidate/package/runtimes/win-x64/native/libSkiaSharp.dll");
  oto::visual::VisualEngine engine;oto::ObsVisualRenderer renderer;unsigned captures=0;
  const AuthoredGlyphRaster raster=[&](const QList<QGlyphRun>& runs,QSize size,QColor color,QPointF origin){return native.raster(runs,size,color,origin);};
  uint64_t identity=1;
  QJsonArray measurements;
  for(const auto value:cases){
   const auto item=value.toObject();AuthoredTextStyle style;
   if(item.contains("family")){
    auto families=item["family"].toString().split(',');
    for(auto &family:families){family=family.trimmed();family.remove('"');family.remove('\'');}
    style.family=families.takeFirst();style.fallbackFamilies=families;
   }
   style.italic=item["italic"].toBool();
   style.pixelSize=item["pixelSize"].toInt();style.weight=static_cast<QFont::Weight>(item["weight"].toInt());
   style.letterSpacing=item["spacing"].toDouble();style.lineHeight=item["lineHeight"].toDouble();style.baseline=item["baseline"].toDouble();
   if(!item.contains("family")||style.family==QStringLiteral("Consolas"))style.fallbackFamilies={QStringLiteral("Courier New"),QStringLiteral("Microsoft YaHei"),QStringLiteral("Segoe UI Emoji")};
   const auto rgb=QRegularExpression(QStringLiteral("rgb\\((\\d+), (\\d+), (\\d+)\\)")).match(item["color"].toString());
   if(!rgb.hasMatch())throw std::runtime_error("Invalid color");
   style.color=QColor(rgb.captured(1).toInt(),rgb.captured(2).toInt(),rgb.captured(3).toInt());
   const int padding=item["padding"].toInt();oto::visual::FramePtr shadowFrame;
   if(padding<0||padding>32)throw std::runtime_error("Invalid authored padding");
   style.inkPadding=padding;
   const auto prepared=prepareAuthoredText(engine,item["text"].toString(),style,raster);
   measurements.append(QJsonObject{{"id",item["id"]},{"nativeAdvance",prepared.advance},{"browserAdvance",item["advance"]},{"families",QJsonArray::fromStringList(prepared.resolvedFamilies)}});
   const QSize size(std::min(1024,item["width"].toInt()),item["height"].toInt());
   if(item["shadowRadius"].toDouble()>0){
    const QColor shadowColor(item["shadowColor"].toString());
    auto shadowStyle=style;shadowStyle.color=Qt::white;
    // Skia127 PreprocessRec explicitly disables mask preblend for blur.
    const AuthoredGlyphRaster coverageRaster=[&](const QList<QGlyphRun>& runs,QSize dimensions,QColor color,QPointF origin){return native.raster(runs,dimensions,color,origin,false);};
    const auto shadowText=prepareAuthoredText(engine,item["text"].toString(),shadowStyle,coverageRaster);
    // Text just outside the captured viewport still contributes blur at its
    // edge. Keep a right gutter before the GPU target performs final clipping.
    QImage mask(QSize(size.width()+padding,size.height()),QImage::Format_RGBA8888_Premultiplied);mask.fill(Qt::transparent);
    {QPainter painter(&mask);for(const auto &tile:shadowText.tiles){painter.save();painter.setClipRect(QRect(tile.contentX+padding,0,tile.contentWidth,mask.height()));painter.drawImage(tile.x+padding,padding-shadowText.inkPadding,tile.pixels->image);painter.restore();}}
    shadowFrame=engine.prepareRaster(native.rasterShadow(mask,shadowColor,item["shadowRadius"].toDouble()));
   }
   for(int matte:{0,255}){
    const auto image=renderer.renderOffscreen(size,[&]{
     renderer.drawSolidRect(QRectF(QPointF{},QSizeF(size)),QColor(matte,matte,matte));
     renderer.composeGroup(identity,size,1,[&]{if(shadowFrame)renderer.draw(shadowFrame);for(const auto &tile:prepared.tiles)renderer.draw(tile.pixels,true,{static_cast<float>(tile.x+padding),static_cast<float>(padding-prepared.inkPadding)});});
     renderer.drawGroupSrgb(identity);
    });
    if(!image.save(output+'/'+item["id"].toString()+(matte?"-white.png":"-black.png")))throw std::runtime_error("Capture failed");
    ++captures;
   }
   ++identity;
  }
  renderer.clear();
  QFile metrics(output+"/font-metrics.json");if(!metrics.open(QIODevice::WriteOnly))throw std::runtime_error("Font metrics unavailable");
  const auto bytes=QJsonDocument(measurements).toJson();if(metrics.write(bytes)!=bytes.size())throw std::runtime_error("Incomplete font metrics");
  if(captures!=static_cast<unsigned>(cases.size()*2)||native.colorLibraryLoads()>1||renderer.stats().bytes)throw std::runtime_error("Capture/lifecycle contract failed");
  std::snprintf(evidence,sizeof(evidence),"PASS checks=%u; actual_GPU_captures=%u; lazy_shared_color_library=%u; graphics_caches_cleared=1; pixel_gate_separate=1",captures+2,captures,native.colorLibraryLoads());
  status.store(0,std::memory_order_release);
 }catch(const std::exception &e){std::snprintf(evidence,sizeof(evidence),"FAIL %s",e.what());status.store(255,std::memory_order_release);}
}
}
extern "C" __declspec(dllexport) int playing_visual_probe_start() noexcept {int expected=-2;if(!qobject_cast<QGuiApplication*>(QCoreApplication::instance())||!status.compare_exchange_strong(expected,-1))return 2;return QMetaObject::invokeMethod(qApp,[]{run();},Qt::QueuedConnection)?0:3;}
extern "C" __declspec(dllexport) int playing_visual_probe_step() noexcept {const int value=status.load(std::memory_order_acquire);return value<0?1:value;}
extern "C" __declspec(dllexport) const char* playing_visual_probe_evidence() noexcept {return status.load(std::memory_order_acquire)<0?"pending":evidence;}
