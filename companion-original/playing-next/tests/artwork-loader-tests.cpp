// SPDX-License-Identifier: MIT
#include "playing/visual/artwork-loader.hpp"
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QElapsedTimer>
#include <QThread>
#include <iostream>
#include <thread>
#include <stdexcept>
using namespace playing;
using namespace playing::visual;
namespace {
unsigned checks=0;
void check(bool value,const char *why) {++checks;if(!value)throw std::runtime_error(why);}
template<class F> void until(F ready) {
    QElapsedTimer deadline;deadline.start();
    while(!ready()&&deadline.elapsed()<5000){QCoreApplication::processEvents();QThread::msleep(1);}
    check(ready(),"Async completion timeout");
}
core::ImageReference local(const QString &path,std::string version="1") {
    return {core::ImageKind::LocalFile,path.toUtf8().toStdString(),std::move(version)};
}
core::SnapshotPtr track(std::optional<core::ImageReference> cover={}) {
    core::PlayingCore state;state.start();core::TrackState song{"Title","Artist"};
    if(cover)song.artwork={core::ArtworkStatus::Ready,cover};
    state.observe({core::Playback::Playing,song});return state.snapshot();
}
void write(const QString &path,const QByteArray &bytes) {
    QFile file(path);check(file.open(QIODevice::WriteOnly),"Fixture open");check(file.write(bytes)==bytes.size(),"Fixture write");
}
}
int main(int argc,char **argv) {
    QGuiApplication app(argc,argv);
    try {
        QTemporaryDir dir;check(dir.isValid(),"Fixture directory");
        const auto redPath=dir.filePath("red.png"),bluePath=dir.filePath("blue.png");
        QImage red(64,32,QImage::Format_ARGB32);red.fill(QColor(255,0,0,128));
        QImage blue(32,64,QImage::Format_ARGB32);blue.fill(Qt::blue);
        check(red.save(redPath)&&blue.save(bluePath),"PNG fixtures");
        const auto redRef=local(redPath),blueRef=local(bluePath);
        ArtworkLoader loader;
        const auto snapshot=track(redRef);ArtworkResult first,second;int delivered=0;
        const auto callback=[&](const ArtworkResult &value){check(QThread::currentThread()==app.thread(),"Delivery thread");first=value;++delivered;};
        loader.request(1,snapshot,blueRef,true,false,callback);
        check(loader.stats().reads==0&&loader.stats().consumers==0,"Inactive acquisition");
        loader.request(1,{},blueRef,true,true,callback);check(loader.stats().reads==0,"Null acquisition");
        loader.request(1,snapshot,blueRef,true,true,callback);
        loader.request(2,snapshot,blueRef,true,true,[&](const auto &value){second=value;++delivered;});
        check(delivered==0&&loader.stats().reads==1,"Coalescing/queued delivery");
        until([&]{return delivered==2;});
        check(first.outcome==ImageOutcome::Ready&&!first.usedProfile,"Cover priority");
        check(first.trackRevision==snapshot->trackRevision&&first.image==second.image,"Shared result/token");
        check(first.image->image.size()==QSize(64,32),"Unrequested enlargement");
        check(first.image->image.format()==QImage::Format_RGBA8888_Premultiplied,"Pixel encoding");
        check(first.image->image.pixelColor(0,0).alpha()==128,"Alpha lost");
        loader.request(1,snapshot,{},false,true,callback);check(delivered==2,"Cache callback synchronous");
        until([&]{return delivered==3;});check(loader.stats().reads==1&&loader.stats().hits==1,"Cache miss");
        const auto retained=first.image;
        loader.request(1,track(),blueRef,true,true,callback);until([&]{return delivered==4;});
        check(first.usedProfile&&first.image->image.pixelColor(0,0)==QColor(Qt::blue),"Missing fallback");
        const auto before=loader.stats().reads;
        loader.request(1,track(),blueRef,false,true,callback);until([&]{return delivered==5;});
        check(first.outcome==ImageOutcome::Missing&&!first.image&&loader.stats().reads==before,"Disabled fallback");
        const auto broken=local(dir.filePath("broken.png"));write(QString::fromUtf8(broken.location),"bad image");
        loader.request(1,track(broken),blueRef,true,true,callback);until([&]{return delivered==6;});
        check(first.outcome==ImageOutcome::Ready&&first.usedProfile,"Decode failure fallback");
        const auto failedReads=loader.stats().reads;
        loader.request(1,track(broken),{},false,true,callback);until([&]{return delivered==7;});
        check(first.outcome!=ImageOutcome::Ready&&loader.stats().reads==failedReads,"Negative cache");
        check(red.save(QString::fromUtf8(broken.location)),"Repair fixture");
        loader.request(1,track(broken),{},false,true,callback);until([&]{return delivered==8;});
        check(!first.image&&loader.stats().reads==failedReads,"Implicit file polling");
        auto repaired=broken;repaired.version="2";
        loader.request(1,track(repaired),{},false,true,callback);until([&]{return delivered==9;});
        check(first.outcome==ImageOutcome::Ready&&loader.stats().reads==failedReads+1,"Version invalidation");
        // Same track, changed profile: cached old callback must be suppressed.
        const auto unknown=track();int old=0;ArtworkResult latest;bool newDone=false;
        loader.request(10,unknown,redRef,true,true,[&](const auto &){++old;});
        loader.request(10,unknown,blueRef,true,true,[&](const auto &value){latest=value;newDone=true;});
        until([&]{return newDone;});check(old==0&&latest.image->image.pixelColor(0,0)==QColor(Qt::blue),"Profile generation stale");
        loader.request(10,snapshot,{},false,true,[&](const auto &){++old;});loader.cancel(10);
        QCoreApplication::processEvents();check(old==0,"Cancelled cached callback");
        core::PlayingCore paused;paused.start();paused.observe({core::Playback::Paused,{}});
        const auto reads=loader.stats().reads;
        loader.request(10,paused.snapshot(),redRef,true,true,callback);check(loader.stats().reads==reads,"Paused I/O");
        const core::ImageReference remote{core::ImageKind::Https,"https://example.test/cover.png#fragment","1"};
        loader.request(1,track(remote),{},false,true,callback);until([&]{return delivered==10;});
        check(first.outcome!=ImageOutcome::Ready&&loader.stats().reads==reads+1,"Unsafe HTTPS reference accepted");
        loader.request(1,track(remote),redRef,true,true,callback);until([&]{return delivered==11;});
        check(first.usedProfile&&first.outcome==ImageOutcome::Ready,"Unsupported fallback");
        const auto hugePath=dir.filePath("huge.png");QImage huge(5000,1,QImage::Format_RGB32);huge.fill(Qt::green);check(huge.save(hugePath),"Dimensions fixture");
        loader.request(1,track(local(hugePath)),{},false,true,callback);until([&]{return delivered==12;});check(!first.image,"Excess dimensions accepted");
        const auto largePath=dir.filePath("large.png");write(largePath,QByteArray(4*1024*1024+1,'x'));
        loader.request(1,track(local(largePath)),{},false,true,callback);until([&]{return delivered==13;});check(!first.image,"Oversized bytes accepted");
        const auto scaledPath=dir.filePath("scaled.png");QImage scaled(2048,1024,QImage::Format_RGB32);scaled.fill(Qt::green);check(scaled.save(scaledPath),"Scaled fixture");
        loader.request(1,track(local(scaledPath)),{},false,true,callback);until([&]{return delivered==14;});check(first.image&&first.image->image.size()==QSize(1024,512),"Resident bound");
        bool wrongThread=false;std::thread foreign([&]{try{loader.stats();}catch(const std::logic_error &){wrongThread=true;}});foreign.join();check(wrongThread,"Foreign owner accepted");
        loader.clear();check(loader.stats().bytes==0&&loader.stats().entries==0&&retained->image.size()==QSize(64,32),"Clear invalidated retained data");
        // Worker results are queued, so replacement before pumping is deterministic.
        old=0;newDone=false;
        loader.request(20,track(redRef),{},false,true,[&](const auto &){++old;});
        loader.request(20,track(blueRef),{},false,true,[&](const auto &value){latest=value;newDone=true;});
        until([&]{return newDone&&loader.stats().pending==0;});
        check(old==0&&latest.image->image.pixelColor(0,0)==QColor(Qt::blue),"Old track published");
        loader.clear();loader.request(21,snapshot,{},false,true,[&](const auto &){++old;});loader.clear();
        until([&]{return loader.stats().pending==0;});QCoreApplication::processEvents();
        check(old==0&&loader.stats().entries==0,"Clear repopulated by stale worker");
        bool reopened=false;
        loader.request(22,snapshot,{},false,true,[&](const auto &){++old;});loader.clear();
        loader.request(22,snapshot,{},false,true,[&](const auto &value){check(value.outcome==ImageOutcome::Ready,"Reopen during clear lost image");reopened=true;});
        until([&]{return reopened;});check(old==0,"Reopen delivered obsolete callback");
        ArtworkLoader tiny(8192,1);int count=0;
        tiny.request(1,snapshot,{},false,true,[&](const auto &){++count;});until([&]{return count==1;});
        tiny.request(2,track(blueRef),{},false,true,[&](const auto &){++count;});until([&]{return count==2;});
        check(tiny.stats().entries==1&&tiny.stats().bytes<=8192&&tiny.stats().evictions==1,"LRU bounds");
        {ArtworkLoader closing;closing.request(1,snapshot,{},false,true,[&](const auto &){++old;});}
        QCoreApplication::processEvents();check(old==0,"Destroyed owner callback");
        check(loader.stats().pending==0&&loader.stats().consumers==0,"Demand not released");
        loader.request(1,track(core::ImageReference{core::ImageKind::Asset,"opaque-cover","1"}),{},false,true,callback);
        until([&]{return delivered==15;});check(first.outcome==ImageOutcome::Unsupported,"Opaque asset interpreted as path");
        const auto svgPath=dir.filePath("static.svg");
        write(svgPath,"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"10\" height=\"10\"><rect width=\"10\" height=\"10\"/></svg>");
        loader.request(1,track(local(svgPath)),{},false,true,callback);until([&]{return delivered==16;});
        check(first.image&&first.image->image.size()==QSize(10,10),"Static SVG not rendered");
        const auto absent=local(dir.filePath("absent.png"));
        loader.request(1,track(absent),absent,true,true,callback);until([&]{return delivered==17;});
        check(first.outcome==ImageOutcome::Failed&&first.usedProfile&&!first.image,"Broken cover and profile manufactured image");
        ArtworkLoader bounded;int rejected=0,accepted=0;
        for(int i=0;i<8;++i)bounded.request(static_cast<std::uint64_t>(i),track(local(redPath,std::to_string(i))),{},false,true,[&](const auto &value){check(value.outcome==ImageOutcome::Ready,"Queued image failed");++accepted;});
        bounded.request(99,track(local(redPath,"overflow")),{},false,true,[&](const auto &value){check(value.outcome==ImageOutcome::Capacity,"Queue budget ignored");++rejected;});
        check(bounded.stats().pending==8&&bounded.stats().reads==8,"Worker queue unbounded");
        until([&]{return accepted==8&&rejected==1;});
        check(bounded.stats().pending==0&&bounded.stats().consumers==0,"Bounded queue did not drain");
        for(const auto &body:{QByteArray("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"10\" height=\"10\"><image href=\"https://example.test/image.png\"/></svg>"),QByteArray("<!DOCTYPE svg [<!ENTITY external SYSTEM 'file:///C:/private'>]><svg width='10' height='10'>&external;</svg>"),QByteArray("<svg width='10' height='10'><animate attributeName='x'/></svg>")}) {
            write(svgPath,body);auto unsafe=local(svgPath,std::to_string(checks));bool done=false;
            loader.request(900,track(unsafe),{},false,true,[&](const auto &value){check(!value.image,"External/animated SVG accepted");done=true;});until([&]{return done;});
        }
        ArtworkLoader assets(8192,8,dir.path());bool assetDone=false;
        assets.request(1,track(core::ImageReference{core::ImageKind::Asset,"red.png","1"}),{},false,true,[&](const auto &value){check(value.image&&value.image->image.size()==QSize(64,32),"Package PNG missing");assetDone=true;});until([&]{return assetDone;});
        for(const auto &format:{"jpg","webp"}) {
            const auto filename=dir.filePath(QString("format.")+format);check(blue.save(filename,format),"Format encoder unavailable");bool done=false;
            loader.request(901,track(local(filename)),{},false,true,[&](const auto &value){check(value.image&&value.image->image.size()==QSize(32,64),"JPEG/WebP decoding failed");done=true;});until([&]{return done;});
        }
        std::cout<<"Artwork loader: "<<checks<<" checks PASS\n";return 0;
    } catch(const std::exception &error){std::cerr<<"FAIL "<<checks<<": "<<error.what()<<'\n';return 1;}
}
