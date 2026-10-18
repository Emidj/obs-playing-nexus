// SPDX-License-Identifier: MIT
// Developer probe only; original engine is unchanged and product never links this.
#include "normal-group-prototype.hpp"
#include "adapters/obs-visual-renderer.hpp"
#include <QGuiApplication>
#include <QMetaObject>
#include <QPainter>
#include <atomic>
#include <cstdio>
namespace {
std::atomic<int> result{-2};
char evidence[2048]{};
unsigned checks=0;
void check(bool value,const char *why){++checks;if(!value)throw std::runtime_error(why);}
bool equivalent(const QImage &a,const QImage &b){
    if(a.size()!=b.size())return false;
    for(int y=0;y<a.height();++y)for(int x=0;x<a.width()*4;++x)
        if(std::abs(int(a.constScanLine(y)[x])-int(b.constScanLine(y)[x]))>2)return false;
    return true;
}
void run() noexcept {
    try {
        {
        oto::visual::VisualEngine engine;oto::ObsVisualRenderer renderer;
        playing::test::NormalGroupPrototype groups;
        QImage red(QSize(32,20),QImage::Format_RGBA8888_Premultiplied);red.fill(QColor(255,0,0));
        QImage blue(QSize(16,20),QImage::Format_RGBA8888_Premultiplied);blue.fill(QColor(0,0,255,128));
        const auto redFrame=engine.prepareRaster(red),blueFrame=engine.prepareRaster(blue);
        QImage layer(QSize(40,32),QImage::Format_RGBA8888_Premultiplied);layer.fill(Qt::transparent);
        {QPainter p(&layer);p.drawImage(0,4,red);p.drawImage(8,4,blue);}
        QImage expected(QSize(96,64),QImage::Format_RGBA8888_Premultiplied);expected.fill(Qt::transparent);
        {QPainter p(&expected);p.setOpacity(.5);p.drawImage(5,7,layer);}
        unsigned paints=0;
        const auto paint=[&]{++paints;renderer.draw(redFrame,true,{0,4});renderer.draw(blueFrame,true,{8,4});};
        const auto first=renderer.renderOffscreen(expected.size(),[&]{groups.compose(0,layer.size(),1,paint);groups.draw(0,5,7,1,.5);});
        check(equivalent(first,expected),"Group opacity differs from composed CPU reference");
        check(paints==1&&groups.renders()==1&&groups.allocations()==1,"Initial group counters");
        const auto again=renderer.renderOffscreen(expected.size(),[&]{groups.compose(0,layer.size(),1,paint);groups.draw(0,5,7,1,.5);});
        check(equivalent(again,first),"Cached group pixels changed");
        check(paints==1&&groups.renders()==1&&groups.allocations()==1&&groups.hits()==1,"Static group repainted or allocated");
        check(renderer.stats().uploads==2,"Group cache caused uploads");
        QImage separately(expected.size(),QImage::Format_RGBA8888_Premultiplied);separately.fill(Qt::transparent);
        {QPainter p(&separately);p.setOpacity(.5);p.drawImage(5,11,red);p.drawImage(13,11,blue);}
        check(!equivalent(first,separately),"Fixture did not distinguish group opacity from individual opacity");
        QImage scaled(QSize(120,80),QImage::Format_RGBA8888_Premultiplied);scaled.fill(Qt::transparent);
        {QPainter p(&scaled);p.setRenderHint(QPainter::SmoothPixmapTransform);p.setOpacity(.5);p.drawImage(QRectF(5,7,80,64),layer);}
        const auto scaledCapture=renderer.renderOffscreen(scaled.size(),[&]{groups.draw(0,5,7,2,.5);});
        check(equivalent(scaledCapture,scaled),"Parent scaling differs from composed CPU reference");
        const auto nested=renderer.renderOffscreen(expected.size(),[&]{
            groups.compose(1,expected.size(),1,[&]{groups.draw(0,5,7,1,.5);});groups.draw(1);
        });
        check(equivalent(nested,first),"Nested group changed composition");
        check(groups.allocations()==2&&groups.renders()==2,"Nested group allocation counters");
        renderer.renderOffscreen(expected.size(),[&]{
            groups.compose(0,layer.size(),2,paint);
            check(paints==2&&groups.allocations()==2,"Revision did not invalidate exactly once");
            bool rejected=false;
            try{groups.compose(2,QSize(40,32),1,[&]{groups.compose(2,QSize(40,32),1,paint);});}catch(const std::logic_error &){rejected=true;}
            check(rejected,"Recursive target was accepted");
            groups.compose(2,QSize(40,32),2,paint);groups.draw(2);
            check(groups.renders()==4,"Exception recovery did not repaint");
            rejected=false;try{groups.compose(8,layer.size(),1,paint);}catch(const std::invalid_argument &){rejected=true;}
            check(rejected,"Unbounded slot accepted");
            rejected=false;try{groups.compose(3,QSize(2049,1),1,paint);}catch(const std::invalid_argument &){rejected=true;}
            check(rejected,"Unbounded texture accepted");
            groups.compose(3,QSize(2048,2048),1,[]{});
            const auto boundedBytes=groups.bytes();const auto boundedAllocations=groups.allocations();
            rejected=false;try{groups.compose(4,QSize(2048,2048),1,[]{});}catch(const std::length_error &){rejected=true;}
            check(rejected&&groups.bytes()==boundedBytes&&groups.allocations()==boundedAllocations,"Budget failure mutated allocation accounting");
            const bool srgb=gs_framebuffer_srgb_enabled();
            groups.draw(0,0,0,1,.5);check(gs_framebuffer_srgb_enabled()==srgb,"Group changed caller color state");
            const auto draws=groups.draws();groups.draw(0,0,0,1,0);
            check(groups.draws()==draws,"Invisible group touched drawing");
        });
        check(renderer.stats().uploads==2,"Dynamic group caused new CPU uploads");
        groups.clear();check(groups.bytes()==0,"Group targets not released");renderer.clear();
        std::snprintf(evidence,sizeof(evidence),"PASS checks=%u; group_opacity_CPU_match=1; parent_scale_CPU_match=1; nested_group=1; static_allocations=0; static_repaints=0; child_uploads=2; recursive_rejected=1; exception_recovery=1; budget_failure_atomic=1; invisible_draws=0; targets_cleared=1; prototype_only=1",checks);
        }
        result.store(0,std::memory_order_release);
    }catch(const std::exception &error){std::snprintf(evidence,sizeof(evidence),"FAIL checks=%u; %s",checks,error.what());result.store(255,std::memory_order_release);}
}
}
extern "C" __declspec(dllexport) int playing_visual_probe_start() noexcept {
    if(!qobject_cast<QGuiApplication *>(QCoreApplication::instance()))return 2;
    int expected=-2;if(!result.compare_exchange_strong(expected,-1))return 3;
    if(!QMetaObject::invokeMethod(qApp,[]{run();},Qt::QueuedConnection)){result.store(255);return 4;}return 0;
}
extern "C" __declspec(dllexport) int playing_visual_probe_step() noexcept {const int status=result.load(std::memory_order_acquire);return status<0?1:status;}
extern "C" __declspec(dllexport) const char *playing_visual_probe_evidence() noexcept {return result.load(std::memory_order_acquire)<0?"pending":evidence;}
