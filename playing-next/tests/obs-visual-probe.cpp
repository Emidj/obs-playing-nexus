// SPDX-License-Identifier: MIT
// Developer-only FFI probe, not an OBS source/plugin or distributable payload.
#include "playing/visual/obs-track-renderer.hpp"
#include <QGuiApplication>
#include <QMetaObject>
#include <QPainter>
#include <QThread>
#include <atomic>
#include <cstdio>
#include <stdexcept>
namespace {
std::atomic<int> result{-2};
char evidence[2048]{};
unsigned checks=0;
void check(bool ok,const char *why) {++checks;if(!ok)throw std::runtime_error(why);}
bool equivalent(const QImage &a,const QImage &b) {
    if(a.size()!=b.size())return false;
    const auto left=a.convertToFormat(QImage::Format_RGBA8888_Premultiplied),right=b.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
    for(int y=0;y<a.height();++y)for(int byte=0;byte<a.width()*4;++byte)
        if(std::abs(int(left.constScanLine(y)[byte])-int(right.constScanLine(y)[byte]))>1) return false;
    return true;
}
void run() noexcept {
    using namespace playing;
    try {
        {
        check(QThread::currentThread()==qApp->thread(),"CPU not on GUI");
        core::PlayingCore core;core.start();core.observe({core::Playback::Playing,core::Metadata{u8"Jóga 🎵","Björk"}});
        visual::TrackVisualHost host;visual::TrackLayout layout;layout.canvas=QSize(480,110);
        layout.title.box=QRect(12,8,450,40);layout.title.pixelSize=24;layout.title.color=QColor(255,255,255);
        layout.artist.box=QRect(12,58,450,38);layout.artist.pixelSize=20;layout.artist.color=QColor(90,170,230);
        const auto first=host.prepare(core.snapshot(),layout),second=host.prepare(core.snapshot(),layout);
        check(first->title==second->title&&first->artist==second->artist,"Two consumers not sharing");
        const auto cpu=host.stats();check(cpu.renders==2&&cpu.hits==2,"Unexpected CPU work");
        QImage expected(layout.canvas,QImage::Format_RGBA8888_Premultiplied);expected.fill(Qt::transparent);
        {QPainter painter(&expected);painter.drawImage(first->titlePosition,first->title->image);painter.drawImage(first->artistPosition,first->artist->image);}
        oto::ObsVisualRenderer renderer;
        visual::drawTrack(renderer,first,false);visual::drawTrack(renderer,{});
        check(renderer.stats().uploads==0&&renderer.stats().draws==0,"Inactive touched GPU");
        const auto capture=renderer.renderOffscreen(layout.canvas,[&]{visual::drawTrack(renderer,first);});
        check(equivalent(capture,expected),"GPU differs from CPU composition");
        check(renderer.stats().uploads==2&&renderer.stats().draws==2,"Initial uploads/draws");
        const auto repeated=renderer.renderOffscreen(layout.canvas,[&]{visual::drawTrack(renderer,second);});
        check(equivalent(repeated,capture),"Repeated draw changed output");
        check(renderer.stats().uploads==2&&renderer.stats().hits==2&&renderer.stats().draws==4,"Repeated draw uploaded");
        core.observe({core::Playback::Playing,core::Metadata{u8"Jóga 🎵","Changed artist"}});
        const auto changed=host.prepare(core.snapshot(),layout);check(changed->title==first->title,"Title re-prepared");
        const auto updated=renderer.renderOffscreen(layout.canvas,[&]{visual::drawTrack(renderer,changed);});
        check(!equivalent(updated,capture),"Artist change not drawn");
        check(host.stats().renders==3&&renderer.stats().uploads==3,"Artist invalidation not isolated");
        const auto before=renderer.stats();const auto blank=renderer.renderOffscreen(layout.canvas,[&]{visual::drawTrack(renderer,changed,false);});
        QImage transparent(layout.canvas,QImage::Format_RGBA8888_Premultiplied);transparent.fill(Qt::transparent);
        check(equivalent(blank,transparent),"Inactive did not stay transparent");
        check(renderer.stats().uploads==before.uploads&&renderer.stats().draws==before.draws,"Inactive repeated GPU work");
        check(host.stats().renders==3,"Draw changed CPU cache");
        core.observe({core::Playback::Paused,std::nullopt});check(!host.prepare(core.snapshot(),layout),"Paused rendered");
        core.stop();check(!host.prepare(core.snapshot(),layout),"Stopped rendered");
        renderer.clear();check(renderer.stats().entries==0&&renderer.stats().bytes==0,"GPU clear failed");
        host.clear();check(host.stats().entries==0&&host.stats().bytes==0,"CPU clear failed");
        check(!first->title->image.isNull()&&first->snapshot->track,"Retained frames invalidated");
        // Renderer destructor repeats safe clear before returning to OBS shutdown.
        std::snprintf(evidence,sizeof(evidence),"PASS checks=%u; cpu_renders=3; gpu_uploads=3; repeated_draw_uploads=0; inactive_draws=0; GUI_prepare=1; GPU_CPU_match=1; caches_cleared=1",checks);
        }
        // Publish completion only after all frames, host and renderer destructors.
        result.store(0,std::memory_order_release);
    }catch(const std::exception &e){std::snprintf(evidence,sizeof(evidence),"FAIL checks=%u; %s",checks,e.what());result.store(255,std::memory_order_release);}
}
}
extern "C" __declspec(dllexport) int playing_visual_probe_start() noexcept {
    if(!qobject_cast<QGuiApplication *>(QCoreApplication::instance()))return 2;
    int initial=-2;if(!result.compare_exchange_strong(initial,-1))return 3;
    // Never wait on GUI while a scripting/video callback might own graphics.
    if(!QMetaObject::invokeMethod(qApp,[]{run();},Qt::QueuedConnection)){result.store(255);return 4;}
    return 0;
}
extern "C" __declspec(dllexport) int playing_visual_probe_step() noexcept {const auto value=result.load(std::memory_order_acquire);return value<0?1:value;}
extern "C" __declspec(dllexport) const char *playing_visual_probe_evidence() noexcept {return result.load(std::memory_order_acquire)<0?"pending":evidence;}
