// SPDX-License-Identifier: MIT
// Candidate shared engine only. External engine and product remain unchanged.
#include "obs-visual-renderer.hpp"
#include <obs.h>
#include <QGuiApplication>
#include <QMetaObject>
#include <QPainter>
#include <atomic>
#include <cstdio>
#include <cmath>
namespace {
std::atomic<int> result{-2};char evidence[2048]{};unsigned checks=0;
void check(bool v,const char *why){++checks;if(!v)throw std::runtime_error(why);}
bool equivalent(const QImage &a,const QImage &b){if(a.size()!=b.size())return false;for(int y=0;y<a.height();++y)for(int x=0;x<a.width()*4;++x)if(std::abs(int(a.constScanLine(y)[x])-int(b.constScanLine(y)[x]))>2)return false;return true;}
void run() noexcept {
 try {
  {
  oto::visual::VisualEngine engine;oto::ObsVisualRenderer renderer;
  QImage red(QSize(32,20),QImage::Format_RGBA8888_Premultiplied);red.fill(QColor(255,0,0));
  QImage blue(QSize(16,20),QImage::Format_RGBA8888_Premultiplied);blue.fill(QColor(0,0,255,128));
  const auto redFrame=engine.prepareRaster(red),blueFrame=engine.prepareRaster(blue);
  QImage layer(QSize(40,32),QImage::Format_RGBA8888_Premultiplied);layer.fill(Qt::transparent);
  {QPainter p(&layer);p.drawImage(0,4,red);p.drawImage(8,4,blue);}
  QImage expected(QSize(96,64),QImage::Format_RGBA8888_Premultiplied);expected.fill(Qt::transparent);
  {QPainter p(&expected);p.setOpacity(.5);p.drawImage(5,7,layer);}
  unsigned paints=0;
  const auto paint=[&]{++paints;renderer.draw(redFrame,true,{0,4});renderer.draw(blueFrame,true,{8,4});};
  const auto first=renderer.renderOffscreen(expected.size(),[&]{renderer.composeGroup(500,layer.size(),1,paint);renderer.drawGroup(500,5,7,1,.5);});
  check(equivalent(first,expected),"Group opacity differs from CPU");
  check(paints==1&&renderer.groupStats().renders==1&&renderer.groupStats().allocations==1,"Initial group counters");
  const auto again=renderer.renderOffscreen(expected.size(),[&]{renderer.composeGroup(500,layer.size(),1,paint);renderer.drawGroup(500,5,7,1,.5);});
  check(equivalent(again,first),"Cached pixels changed");
  check(paints==1&&renderer.groupStats().renders==1&&renderer.groupStats().allocations==1&&renderer.groupStats().hits==1,"Static group did work");
  check(renderer.stats().uploads==2,"Static group uploaded");
  check(renderer.stats().bytes==renderer.groupStats().bytes+size_t(red.sizeInBytes()+blue.sizeInBytes()),"Group absent from combined budget");
  const auto nested=renderer.renderOffscreen(expected.size(),[&]{renderer.composeGroup(600,expected.size(),1,[&]{renderer.composeGroup(500,layer.size(),1,paint);renderer.drawGroup(500,5,7,1,.5);});renderer.drawGroup(600);});
  check(equivalent(nested,first),"Nested group pixels changed");
  renderer.renderOffscreen(expected.size(),[&]{
   renderer.composeGroup(500,layer.size(),2,paint);check(paints==2&&renderer.groupStats().allocations==2,"Content revision reallocated");
   bool rejected=false;try{renderer.composeGroup(700,layer.size(),1,[&]{renderer.composeGroup(700,layer.size(),1,paint);});}catch(const std::logic_error &){rejected=true;}
   check(rejected,"Recursive identity accepted");renderer.composeGroup(700,layer.size(),2,paint);renderer.drawGroup(700);
   const auto before=renderer.groupStats();renderer.composeGroup(900,layer.size(),1,paint,false);renderer.drawGroup(900,0,0,1,1,false);renderer.drawGroup(700,0,0,1,0);
   check(renderer.groupStats().renders==before.renders&&renderer.groupStats().draws==before.draws&&renderer.groupStats().allocations==before.allocations,"Inactive group did work");
   QImage straight(QSize(8,8),QImage::Format_RGBA8888);straight.fill(QColor(255,255,255,128));const auto straightFrame=engine.prepareSrgbRaster(straight);
   rejected=false;try{renderer.composeGroup(800,QSize(8,8),1,[&]{renderer.draw(straightFrame);});}catch(const std::invalid_argument &){rejected=true;}
   check(rejected,"Wrong alpha/color encoding accepted");
   renderer.draw(straightFrame);check(renderer.stats().uploads==3,"Group exception leaked encoding scope");
   for(uint64_t key=1000;key<1040;++key)renderer.composeGroup(key,QSize(1,1),1,[]{});
   check(renderer.groupStats().entries<=32&&renderer.groupStats().evictions>0,"Group identity cache unbounded");
   for(uint64_t key=2000;key<2006;++key)renderer.composeGroup(key,QSize(2048,2048),1,[]{});
   check(renderer.stats().bytes<=oto::ObsVisualRenderer::textureBudgetBytes&&renderer.groupStats().bytes<=renderer.stats().bytes,"Combined memory budget exceeded");
   check(renderer.stats().entries==0&&renderer.groupStats().evictions>0,"Memory pressure did not evict both resource types");
  });
  renderer.clear();check(renderer.stats().bytes==0&&renderer.groupStats().bytes==0&&renderer.groupStats().entries==0,"Group clear leaked resource accounting");
#ifdef PLAYING_GROUP_SRGB
  QImage colors(QSize(32,16),QImage::Format_RGBA8888_Premultiplied);
  for(int y=0;y<16;++y)for(int x=0;x<32;++x)colors.setPixelColor(x,y,QColor(x*8,y*16,80,(x+y*2)*5%256));
  const auto colorFrame=engine.prepareRaster(colors);unsigned srgbPaints=0;
  const auto decode=[](double s){return s<=.04045?s/12.92:std::pow((s+.055)/1.055,2.4);};
  const auto encode=[](double s){return s<=.0031308?12.92*s:1.055*std::pow(s,1/2.4)-.055;};
  for(int matte:{0,255})for(double opacity:{1.0,.5}){
   const auto image=renderer.renderOffscreen(colors.size(),[&]{
    renderer.drawSolidRect(QRectF(QPointF{},QSizeF(colors.size())),QColor(matte,matte,matte));
    renderer.composeGroup(4000,colors.size(),1,[&]{++srgbPaints;renderer.draw(colorFrame);});
    const bool before=gs_framebuffer_srgb_enabled();renderer.drawGroupSrgb(4000,0,0,1,static_cast<float>(opacity));
    check(gs_framebuffer_srgb_enabled()==before,"sRGB group changed caller color state");
   });
   for(int y=0;y<16;++y)for(int x=0;x<32;++x){const auto *pixel=colors.constScanLine(y)+x*4;const auto actual=image.pixelColor(x,y);
    for(int channel=0;channel<3;++channel){const double linear=decode(pixel[channel]/255.0)*opacity+matte/255.0*(1-pixel[3]/255.0*opacity);
     const int expectedByte=static_cast<int>(std::round(255*encode(std::min(1.0,linear))));
     const int actualByte=channel==0?actual.red():channel==1?actual.green():actual.blue();
     check(std::abs(expectedByte-actualByte)<=2,"Premultiplied sRGB group differs from Browser color contract");
    }check(actual.alpha()==255,"Opaque matte acquired incorrect alpha");
   }
  }
  check(srgbPaints==1&&renderer.groupStats().allocations==1&&renderer.stats().uploads==1,"sRGB output invalidated identical group or uploaded raster");
  renderer.renderOffscreen(colors.size(),[&]{
   bool rejected=false;try{renderer.composeGroup(4001,colors.size(),1,[&]{renderer.drawGroupSrgb(4000);});}catch(const std::logic_error &){rejected=true;}
   check(rejected,"Decoded group entered an encoded parent");
   renderer.composeGroup(4001,colors.size(),2,[]{});renderer.drawGroup(4001);
   const auto before=renderer.groupStats();renderer.drawGroupSrgb(9999,0,0,1,1,false);renderer.drawGroupSrgb(9999,0,0,1,0);
   check(renderer.groupStats().draws==before.draws,"Invisible sRGB group touched graphics");
  });
  renderer.clear();check(renderer.stats().bytes==0&&renderer.groupStats().bytes==0,"sRGB resources retained after clear");
#endif
#ifdef PLAYING_GROUP_APPLIED
  const char *scope="shared_applied=1";
#else
  const char *scope="shared_candidate_only=1";
#endif
  std::snprintf(evidence,sizeof(evidence),"PASS checks=%u; %s; opacity_CPU_match=1; nested_groups=1; static_repaints=0; static_allocations=0; stable_identity_LRU=1; combined_budget=67108864; wrong_encoding_rejected=1; exceptions_recovered=1; caches_cleared=1",checks,scope);
  }
  result.store(0,std::memory_order_release);
 }catch(const std::exception &e){std::snprintf(evidence,sizeof(evidence),"FAIL checks=%u; %s",checks,e.what());result.store(255,std::memory_order_release);}
}
}
extern "C" __declspec(dllexport) int playing_visual_probe_start() noexcept{if(!qobject_cast<QGuiApplication *>(QCoreApplication::instance()))return 2;int expected=-2;if(!result.compare_exchange_strong(expected,-1))return 3;if(!QMetaObject::invokeMethod(qApp,[]{run();},Qt::QueuedConnection)){result.store(255);return 4;}return 0;}
extern "C" __declspec(dllexport) int playing_visual_probe_step() noexcept{const int status=result.load(std::memory_order_acquire);return status<0?1:status;}
extern "C" __declspec(dllexport) const char *playing_visual_probe_evidence() noexcept{return result.load(std::memory_order_acquire)<0?"pending":evidence;}
