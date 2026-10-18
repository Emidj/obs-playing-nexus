// SPDX-License-Identifier: GPL-2.0-or-later
#include "native-terminal-scene.hpp"
#include "playing/visual/terminal-prepared.hpp"
#include "playing/visual/terminal-motion.hpp"
#include "playing/visual/native-glyph-raster.hpp"
#include "obs-visual-renderer.hpp"
#include <QJsonObject>
#include <QPainter>
#include <QLinearGradient>
#include <QFile>
#include <QJsonDocument>

unsigned captureNativeTerminalScenes(const QString &root,const QString &output,const QJsonArray &cases){
 using namespace playing::visual::normal;
 NativeGlyphRasterizer native(root+"/work/phase12l-skia-candidate/package/runtimes/win-x64/native/libSkiaSharp.dll");
 oto::visual::VisualEngine engine;oto::ObsVisualRenderer renderer;unsigned captures=0;uint64_t identity=1;QJsonArray metrics;
 const AuthoredGlyphRaster raster=[&](const QList<QGlyphRun>&runs,QSize size,QColor color,QPointF origin){return native.raster(runs,size,color,origin);};
 for(const auto value:cases){
  const auto item=value.toObject(),state=item["state"].toObject();
  if(item["kind"].toString()!="terminal-scene"||state["width"].toInt()!=400||state["gap"].toInt()!=12)throw std::runtime_error("Terminal candidate currently requires original 400-unit geometry");
  auto track=std::make_shared<playing::core::TrackState>();track->title=state["title"].toString().toUtf8().toStdString();track->artist=state["artist"].toString().toUtf8().toStdString();
  const auto text=prepareTerminalText(engine,track,400,12,state["label"].toString(),raster);const auto &layout=text->layout;
  metrics.append(QJsonObject{{"id",item["id"]},{"promptAdvance",text->prompt.advance},{"titleAdvance",text->rows[1].advance},{"rowX",layout.rows[1].x()},{"artistY",layout.rows[2].y()},{"cursorX",layout.cursor.x()}});
  const double milliseconds=item["time"].toDouble();const auto decoration=terminalDecoration(milliseconds);
  const auto snapped=[](QRectF box){const double x=std::nearbyint(box.left()),y=std::nearbyint(box.top());return QRectF(x,y,std::nearbyint(box.right())-x,std::nearbyint(box.bottom())-y);};
  const auto panel=snapped(layout.panel),inside=snapped(layout.panel.adjusted(1,1,-1,-1));
  QImage canvas(layout.canvas,QImage::Format_RGBA8888_Premultiplied);canvas.fill(Qt::transparent);
  // Static material candidate, CPU prepared before any graphics callback.
  {QPainter painter(&canvas);
   painter.fillRect(panel.adjusted(-4,-4,4,4),QColor(12,28,21,128));
   painter.fillRect(panel,QColor(100,186,131));
   painter.save();painter.setClipRect(inside);
   const double bottom=layout.panel.bottom()-1;
   for(int y=static_cast<int>(inside.top());y<inside.bottom();++y){const double phase=std::fmod(bottom-(y+.5),4.0);painter.fillRect(QRectF(inside.left(),y,inside.width(),1),phase<2?QColor(9,27,16):QColor(13,33,23));}
   painter.restore();
  }
  // CSS inset shadow: blur the complement of the padding box, then clip.
  QImage complement(layout.canvas,QImage::Format_RGBA8888_Premultiplied);complement.fill(Qt::white);
  {QPainter painter(&complement);painter.setCompositionMode(QPainter::CompositionMode_Source);painter.fillRect(inside,Qt::transparent);}
  const auto inset=native.rasterShadow(complement,QColor(1,7,5),20);
  {QPainter painter(&canvas);painter.setClipRect(inside);painter.drawImage(0,0,inset);}
  {QPainter painter(&canvas);
   // Blink127 StyledStrokeData: thin dash=3*width, gap=2*width;
   // SelectBestDashGap minimizes deviation and preserves both endpoints.
   const float length=static_cast<float>(layout.header.width()),dash=3,gap=2;
   const float minimum=std::floor((length+gap)/(dash+gap)),maximum=minimum+1;
   const float lowGap=(length-minimum*dash)/(minimum-1),highGap=(length-maximum*dash)/(maximum-1);
   const float selected=highGap<=0||std::abs(lowGap-gap)<std::abs(highGap-gap)?lowGap:highGap;
   painter.setRenderHint(QPainter::Antialiasing);
   for(float x=static_cast<float>(layout.header.left());x<layout.header.right();x+=dash+selected)painter.fillRect(QRectF(x,layout.header.bottom()-1,std::min(static_cast<double>(dash),layout.header.right()-x),1),QColor(60,117,82));
   painter.setRenderHint(QPainter::Antialiasing,false);
   for(std::size_t i=0;i<layout.bars.size();++i)painter.fillRect(snapped(layout.bars[i]),i<5?QColor(141,226,172):QColor(40,89,59));
   painter.setOpacity(decoration.cursorOpacity);painter.fillRect(snapped(layout.cursor),QColor(163,250,196));painter.setOpacity(1);
  }
  AuthoredTextStyle label;label.pixelSize=11;label.weight=QFont::Normal;label.lineHeight=13;label.baseline=10;label.color=QColor(121,185,149);label.inkPadding=12;
  label.fallbackFamilies={QStringLiteral("Courier New"),QStringLiteral("Microsoft YaHei"),QStringLiteral("Segoe UI Emoji")};
  auto title=label;title.pixelSize=29;title.weight=QFont::Medium;title.lineHeight=32.46875;title.baseline=26;title.color=QColor(163,250,196);
  auto artist=label;artist.pixelSize=13;artist.weight=QFont::Medium;artist.letterSpacing=.6;artist.lineHeight=18.1875;artist.baseline=13;artist.color=QColor(120,196,152);
  auto prompt=label;prompt.pixelSize=30;prompt.lineHeight=36;prompt.baseline=28;prompt.color=QColor(89,164,123);
  std::vector<std::pair<oto::visual::FramePtr,oto::DrawParams>> glyphDraws;
  std::vector<std::pair<oto::visual::FramePtr,oto::DrawParams>> staticDraws;
  const auto paintText=[&](const QString &string,const AuthoredTextStyle &style,const QRectF &box,bool scrolling,bool shadow){
   const QPointF phase=scrolling?QPointF{}:QPointF(box.x()-std::floor(box.x()),box.y()-std::floor(box.y()));
   const double originX=scrolling?std::round(box.x()):std::floor(box.x()),originY=scrolling?std::round(box.y()):std::floor(box.y());
   const AuthoredGlyphRaster phased=[&](const QList<QGlyphRun>&runs,QSize size,QColor color,QPointF origin){return native.raster(runs,size,color,origin+phase);};
   auto prepared=prepareAuthoredText(engine,string,style,phased);
   const double distance=std::max(0.0,std::ceil(prepared.advance*64)/64-std::floor(box.width()+.5));
   const double offset=scrolling?scrollOffset(milliseconds/1000,distance,state["speed"].toDouble(),state["pause"].toDouble()):0;
   const double translatedX=originX;
   if(offset>0){const AuthoredGlyphRaster shifted=[&](const QList<QGlyphRun>&runs,QSize size,QColor color,QPointF origin){return native.raster(runs,size,color,origin+phase-QPointF(offset,0));};prepared=prepareAuthoredText(engine,string,style,shifted);}
   const double clipX=std::round(box.x()),clipY=std::round(box.y());
   const QRectF clip(clipX,clipY,std::round(box.right())-clipX,std::round(box.bottom())-clipY);
   if(shadow){
    auto shadowStyle=style;shadowStyle.color=QColor(99,233,142,68);
    const AuthoredGlyphRaster filtered=[&](const QList<QGlyphRun>&runs,QSize size,QColor color,QPointF origin){return native.rasterGlyphShadow(runs,size,color,origin+phase-QPointF(offset,0),7);};
    const auto shadowText=prepareAuthoredText(engine,string,shadowStyle,filtered);
    for(const auto &tile:shadowText.tiles){oto::DrawParams params;params.x=static_cast<float>(translatedX+tile.x);params.y=static_cast<float>(originY-shadowText.inkPadding);params.clip=clip;glyphDraws.emplace_back(tile.pixels,params);}
   }
   for(const auto &tile:prepared.tiles){oto::DrawParams params;params.x=static_cast<float>(translatedX+tile.x);params.y=static_cast<float>(originY-prepared.inkPadding);if(scrolling)params.clip=clip;(scrolling?glyphDraws:staticDraws).emplace_back(tile.pixels,params);}
  };
  paintText(state["label"].toString(),label,layout.rows[0],true,false);
  paintText(state["title"].toString().toUpper(),title,layout.rows[1],true,true);
  paintText(state["artist"].toString().toUpper(),artist,layout.rows[2],true,false);
  paintText(QStringLiteral("[ ON AIR ]"),label,QRectF(layout.header.right()-text->onAir.advance,34,text->onAir.advance,13),false,false);
  paintText(QStringLiteral(">"),prompt,layout.prompt,false,false);
  oto::visual::FramePtr scanFrame;
  if(decoration.scanOpacity>0){QImage scanCanvas(layout.canvas,QImage::Format_RGBA8888_Premultiplied);scanCanvas.fill(Qt::transparent);{QPainter painter(&scanCanvas);painter.setClipRect(layout.panel.adjusted(1,1,-1,-1));painter.setOpacity(decoration.scanOpacity);
   const auto inner=layout.panel.adjusted(1,1,-1,-1);const double y=inner.top()+inner.height()*decoration.scanTopPercent/100,height=inner.height()*.2;
   QLinearGradient gradient(0,y,0,y+height);gradient.setColorAt(0,Qt::transparent);gradient.setColorAt(.5,QColor(184,255,208));gradient.setColorAt(1,Qt::transparent);painter.fillRect(QRectF(inner.x(),y,inner.width(),height),gradient);}
   scanFrame=engine.prepareRaster(std::move(scanCanvas));
  }
  const auto frame=engine.prepareRaster(std::move(canvas));
  for(int matte:{0,255}){const auto capture=renderer.renderOffscreen(layout.canvas,[&]{renderer.drawSolidRect(QRectF(QPointF{},QSizeF(layout.canvas)),QColor(matte,matte,matte));renderer.composeGroup(identity,layout.canvas,1,[&]{renderer.draw(frame);for(const auto &draw:staticDraws)renderer.draw(draw.first,true,draw.second);if(scanFrame)renderer.draw(scanFrame);for(const auto &draw:glyphDraws)renderer.draw(draw.first,true,draw.second);});renderer.drawGroupSrgb(identity);});
   if(!capture.save(output+'/'+item["id"].toString()+(matte?"-white.png":"-black.png")))throw std::runtime_error("Terminal capture failed");++captures;}
  ++identity;
 }
 renderer.clear();if(renderer.stats().bytes)throw std::runtime_error("Terminal graphics cache not cleared");QFile metricsFile(output+"/terminal-metrics.json");if(!metricsFile.open(QIODevice::WriteOnly)||metricsFile.write(QJsonDocument(metrics).toJson())<0)throw std::runtime_error("Terminal metric evidence failed");return captures;
}
