// SPDX-License-Identifier: MIT
#include "playing/obs/session-host.hpp"
#include "playing/companion/protected-store.hpp"
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QElapsedTimer>
#include <QThread>
#include <iostream>
#include <thread>
#include <stdexcept>
#include <cmath>
namespace {
unsigned checks=0;
bool ready(const std::shared_ptr<playing::obs::Consumer> &consumer){const auto f=consumer->publication.read();return f&&(f->title||!f->scrollingText[0].empty()||(f->normalFrame&&!f->normalFrame->text[1].empty()));}
bool blank(const std::shared_ptr<playing::obs::Consumer> &consumer){return bool(consumer->publication.read())&&!ready(consumer);}
void check(bool ok,const char *why) { ++checks; if(!ok) throw std::runtime_error(why); }
void write(const QString &path,const QByteArray &bytes) {
    QFile file(path); if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()) throw std::runtime_error("Fixture write");
}
template<class F> void await(F predicate) {
    QElapsedTimer clock;clock.start();
    while(!predicate()) { QCoreApplication::processEvents(); QThread::msleep(5); if(clock.elapsed()>5000) throw std::runtime_error("Host timeout"); }
}
void pump(int milliseconds) {
    QElapsedTimer clock;clock.start();
    while(clock.elapsed()<milliseconds) {QCoreApplication::processEvents();QThread::msleep(5);}
}
}
int main(int argc,char **argv) {
    QGuiApplication app(argc,argv);
    using namespace playing;
    try {
        QTemporaryDir temp;check(temp.isValid(),"Temp directory");
        const auto path=temp.filePath("history.txt");
        write(path,QByteArray(u8"21:05 : Björk - Jóga 🎵\r\n"));
        visual::TrackLayout layout;layout.title.box=QRect(12,8,600,46);layout.artist.box=QRect(12,65,600,38);
        auto a=std::make_shared<obs::Consumer>(),b=std::make_shared<obs::Consumer>();
        visual::TrackFramePtr retained;
        {
        obs::SessionHost host;
        check(!host.scheduled()&&host.sessions()==0,"Idle starts readers/timer");
        const providers::Request request{"virtualdj",providers::Mode::History,path.toUtf8().toStdString()};
        auto invalid=layout;invalid.canvas=QSize(1,1);bool rejected=false;
        try {host.attach(request,invalid,a);}catch(const std::invalid_argument &){rejected=true;}
        check(rejected,"Invalid layout attached");
        auto relative=request;relative.historyPath="relative.txt";rejected=false;
        try {host.attach(relative,layout,a);}catch(const std::invalid_argument &){rejected=true;}
        check(rejected,"Relative history accepted");
        const auto first=host.attach(request,layout,a),second=host.attach(request,layout,b);
        rejected=false;try{host.attach(request,layout,a);}catch(const std::invalid_argument &){rejected=true;}
        check(rejected,"Consumer attached twice");
        check(!host.scheduled()&&host.visualStats().renders==0,"Inactive consumer prepared text");
        host.active(first,true);host.active(second,true);
        check(host.sessions()==1&&host.consumers()==2&&host.scheduled(),"Consumers not sharing demand");
        await([&]{return ready(a)&&ready(b);});
        retained=a->publication.read();const auto peer=b->publication.read();
        check(retained->snapshot==peer->snapshot,"Core snapshot not shared");
        check(retained->title==peer->title&&retained->artist==peer->artist,"Visual layers not shared");
        check(retained->snapshot->track->title==u8"Jóga 🎵","Real Unicode history lost");
        check(host.visualStats().renders==2,"Duplicate CPU rendering");
        const auto stats=host.visualStats();pump(350);
        check(host.visualStats().renders==stats.renders&&host.visualStats().hits==stats.hits,"Unchanged state prepared again");
        auto moved=layout;moved.canvas=QSize(800,180);moved.title.box.moveLeft(24);
        const auto reads=host.refreshes();host.configure(first,moved,true,0);
        check(host.refreshes()==reads&&host.sessions()==1,"Layout restarted/refreshed reader");
        check(a->publication.read()->snapshot==retained->snapshot&&b->publication.read()==peer,"Layout altered core/peer");
        check(a->publication.read()->canvas==QSize(800,180)&&a->publication.read()->titlePosition.x()==24,"Geometry update absent");
        check(a->publication.read()->title==retained->title,"Position change rasterized title");
        const auto unchanged=host.visualStats();host.configure(first,moved,true,0);
        check(host.visualStats().renders==unchanged.renders&&host.visualStats().hits==unchanged.hits,"Identical layout touched cache");
        auto bad=moved;bad.title.box.moveLeft(-1);rejected=false;
        try{host.configure(first,bad,true,0);}catch(const std::invalid_argument &){rejected=true;}
        check(rejected&&a->publication.read()->canvas==QSize(800,180),"Invalid geometry mutated active frame");
        host.configure(first,layout,true,0);
        write(path,QByteArray("21:06 : Bjork - Changed song\r\n"));
        await([&]{auto f=a->publication.read();return f&&f->snapshot&&f->snapshot->track&&f->snapshot->track->title=="Changed song";});
        check(retained->snapshot->track->title==u8"Jóga 🎵","Retained snapshot mutated");
        bool crossRead=false,wrongOwner=false;
        std::thread other([&]{crossRead=bool(a->publication.read());try{host.refresh();}catch(const std::logic_error &){wrongOwner=true;}});other.join();
        check(crossRead&&wrongOwner,"Render handoff/owner boundary");
        host.active(first,false);check(!a->publication.read()&&b->publication.read(),"Deactivate did not isolate consumer");
        check(host.sessions()==1&&host.consumers()==1,"Premature shared session release");
        host.active(second,false);check(!b->publication.read()&&!host.scheduled()&&host.sessions()==0,"Last consumer did not stop");
        const auto ticks=host.refreshes();pump(350);host.refresh();
        check(host.refreshes()==ticks&&host.visualStats().entries==0,"Idle still polling/caching");
        host.active(first,true);await([&]{return ready(a);});
        check(a->publication.read()->snapshot->track->title=="Changed song","Reactivation failed");
        QFile::remove(path);await([&]{return blank(a);});
        check(blank(a),"Unavailable must retain only the layout");
        check(a->issue.load()==providers::ProviderIssue::MissingHistory,"Missing history issue not passed to host consumer");
        check(host.failure().empty(),"Host event failure");
        host.detach(first);host.detach(second);check(!host.scheduled()&&host.consumers()==0,"Detach retained demand");
        // Deliberately leave one active consumer: destructor must clear and join.
        write(path,QByteArray("21:08 : Artist - Final song\r\n"));
        const auto final=host.attach(request,layout,a);host.active(final,true);
        await([&]{return ready(a);});
        }
        check(!a->publication.read()&&!b->publication.read(),"Destructor retained publication");
        check(retained&&!retained->title->image.isNull(),"Retained frame destroyed with host");
        {
        obs::SessionHost host;
        const providers::Request request{"virtualdj",providers::Mode::History,path.toUtf8().toStdString()};
        const auto one=host.attach(request,layout,a,false),two=host.attach(request,layout,b,true,250);
        host.active(one,true);host.active(two,true);
        await([&]{return ready(b);});
        check(blank(a)&&host.sessions()==1,"Initial visibility must be per consumer");
        await([&]{return blank(b);});
        check(blank(a),"Expiration reveals hidden initial track");
        write(path,QByteArray("21:09 : Artist - Policy next\r\n"));
        await([&]{return ready(a)&&ready(b);});
        check(a->publication.read()->snapshot==b->publication.read()->snapshot,"Policy duplicated core");
        await([&]{return blank(b);});
        check(bool(a->publication.read()),"Expiration cleared other consumer");
        host.active(one,false);host.active(two,false);pump(300);
        check(!host.scheduled()&&!a->publication.read()&&!b->publication.read(),"Visibility retained inactive demand");
        }
        {
        companion::Profile profile;profile.identity=companion::createIdentity();profile.autoReconnect=true;
        companion::Sender sender;const auto credential=companion::newDeviceCredential();
        check(sender.pair(credential)&&sender.listen(profile.identity,"127.0.0.1",0),"Remote TLS sender");
        profile.receiver=companion::ReceiverSettings{"127.0.0.1",sender.port(),"OBS Playing Companion",companion::certificatePin(profile.identity.certificate),credential};
        const auto protectedPath=temp.filePath("receiver.pcs");
        companion::ProtectedStore(protectedPath).save(companion::encodeProfile(profile));
        core::PlayingCore dj;dj.start();dj.observe({core::Playback::Playing,core::Metadata{u8"Remote 🎵", "DJ"}});sender.publish(dj.snapshot());
        obs::SessionHost host;const auto id=host.remoteProfile(protectedPath);
        check(host.remoteProfile(protectedPath)==id,"Same protected profile not shared");
        const providers::Request request{id,providers::Mode::Automatic,{}};
        const auto one=host.attach(request,layout,a),two=host.attach(request,layout,b);
        check(sender.clientCount()==0&&!host.scheduled(),"Inactive remote connected");
        host.active(one,true);host.active(two,true);
        await([&]{return ready(a)&&ready(b);});
        check(host.remoteConnected(id)&&sender.clientCount()==1&&host.sessions()==1,"Remote consumers duplicate TLS/session");
        const auto remoteFrame=a->publication.read();auto remoteLayout=layout;remoteLayout.title.box.moveLeft(20);
        host.configure(one,remoteLayout,true,0);
        check(sender.clientCount()==1&&host.remoteConnected(id)&&a->publication.read()->snapshot==remoteFrame->snapshot,"Remote layout reconnected or replaced Core");
        check(!host.scheduled(),"Remote-only consumer polls registry");
        check(a->publication.read()->snapshot==b->publication.read()->snapshot,"Remote core not shared");
        check(a->publication.read()->snapshot->track->title==u8"Remote 🎵","Remote metadata missing");
        dj.observe({core::Playback::Paused,std::nullopt});sender.publish(dj.snapshot());
        await([&]{return blank(a)&&blank(b);});
        check(blank(a),"Remote pause must retain only the layout");
        dj.observe({core::Playback::Playing,core::Metadata{"Next remote", "DJ"}});sender.publish(dj.snapshot());
        await([&]{auto frame=a->publication.read();return frame&&frame->snapshot->track->title=="Next remote";});
        host.active(one,false);check(host.remoteConnected(id),"Remote peer disconnected early");
        host.active(two,false);await([&]{return sender.clientCount()==0;});
        check(!host.remoteConnected(id)&&!host.scheduled()&&host.sessions()==0,"Inactive remote retained transport");
        host.active(one,true);await([&]{return ready(a);});
        check(sender.revoke(credential.id),"Sender revoke failed");
        await([&]{return blank(a)&&!host.remoteConnected(id);});
        check(blank(a),"Revoked remote retained text");
        host.detach(one);host.detach(two);
        profile.receiver.reset();profile.autoReconnect=false;
        companion::ProtectedStore(protectedPath).save(companion::encodeProfile(profile));
        bool rejected=false;try{host.remoteProfile(protectedPath);}catch(const std::invalid_argument &){rejected=true;}
        check(rejected,"Forgotten profile acquired remote");
        }
        {
        obs::SessionHost host;
        write(path,QByteArray("21:10 : Artist - First normal\r\n"));
        const providers::Request request{"virtualdj",providers::Mode::History,path.toUtf8().toStdString()};
        auto normal=layout;normal.normalPreset=1;normal.canvas.setWidth(720);
        const auto one=host.attach(request,normal,a),two=host.attach(request,normal,b);
        host.active(one,true);host.active(two,true);
        await([&]{return ready(a)&&ready(b);});
        const auto old=a->publication.read();
        check(old->normalFrame&&old->canvas==QSize(720,166),"NORMAL prepared in shared host");
        check(old->normalFrame==b->publication.read()->normalFrame,"NORMAL duplicated font preparation across consumers");
        check(old->normalFrame->background==b->publication.read()->normalFrame->background,"NORMAL common background cache");
        check(old->normalFrame->text[1][0].pixels==b->publication.read()->normalFrame->text[1][0].pixels,"NORMAL shared glyph cache");
        pump(250);check(a->publication.read()==old&&!host.animationScheduled(),"Static NORMAL republishes immutable frame");
        write(path,QByteArray("21:11 : Artist - Second normal\r\n"));
        await([&]{auto frame=a->publication.read();return frame&&frame->normalSample.transitioning;});
        check(a->publication.read()->normalFrame->track->title=="First normal"&&host.animationScheduled(),"OUT prematurely swaps track");
        check(a->publication.read()->snapshot==old->snapshot&&a->publication.read()->snapshot->track==a->publication.read()->normalFrame->track,"OUT mixed pending snapshot with displayed track");
        await([&]{auto frame=a->publication.read();return frame&&frame->normalFrame->track->title=="Second normal"&&!frame->normalSample.transitioning;});
        check(old->normalFrame->track->title=="First normal","NORMAL old immutable frame changed");
        check(a->publication.read()->snapshot->track==a->publication.read()->normalFrame->track,"IN snapshot differs from displayed track");
        const auto completed=a->publication.read();pump(250);check(a->publication.read()==completed,"Completed IN retains animation timer");
        normal.normalPreset=2;host.configure(one,normal,true,0);
        check(a->publication.read()->canvas==QSize(720,158)&&host.sessions()==1,"Air configuration changes shared reader");
        normal.normalTransition=visual::normal::Transition::Slide;normal.normalDuration=200;normal.normalGap=24;
        normal.normalSpeed=80;normal.normalPause=0;host.configure(one,normal,true,0);
        check(a->publication.read()->canvas==QSize(720,182)&&a->publication.read()->normalSpeed==80&&a->publication.read()->normalPause==0,"NORMAL authored movement options missing");
        auto invalidNormal=normal;invalidNormal.normalGap=81;bool rejected=false;
        try{host.configure(one,invalidNormal,true,0);}catch(const std::invalid_argument&){rejected=true;}
        check(rejected&&a->publication.read()->canvas==QSize(720,182),"Invalid NORMAL motion changed publication");
        write(path,QByteArray("21:12 : Artist - Third normal\r\n"));
        await([&]{auto frame=a->publication.read();return frame&&frame->normalSample.transitioning&&frame->normalSample.transition.offsetYPercent!=0;});
        check(a->publication.read()->normalSample.transition.scale==1,"Slide option ignored");
        await([&]{auto frame=a->publication.read();return frame&&frame->normalFrame->track->title=="Third normal"&&!frame->normalSample.transitioning;});
        host.active(one,false);host.active(two,false);const auto ticks=host.refreshes();pump(250);
        check(!a->publication.read()&&!b->publication.read()&&host.refreshes()==ticks&&!host.animationScheduled(),"Hidden NORMAL retains frames/work");
        normal.normalPreset=3;host.configure(one,normal,true,0);host.active(one,true);
        await([&]{return a->publication.read()&&host.animationScheduled();});
        const auto redesigned=a->publication.read();pump(100);
        check(a->publication.read()!=redesigned&&a->publication.read()->normalFrame==redesigned->normalFrame,"Decoration republishes samples without rasterizing again");
        check(host.sessions()==1&&redesigned->normalFrame->animatedDecoration,"Cantilever shares native provider demand");
        host.active(one,false);pump(100);
        check(!host.animationScheduled()&&!host.scheduled()&&!a->publication.read(),"Hidden Cantilever stops animation and provider timer");
        }
        {
        const auto imagePath=temp.filePath("profile.png");QImage avatar(32,64,QImage::Format_ARGB32);avatar.fill(Qt::cyan);check(avatar.save(imagePath),"Profile fixture");
        obs::SessionHost host;auto withImage=layout;withImage.artworkEnabled=true;withImage.artworkBox=QRect(0,0,100,100);
        withImage.streamerProfile=core::ImageReference{core::ImageKind::LocalFile,imagePath.toUtf8().toStdString(),"1"};
        const providers::Request request{"virtualdj",providers::Mode::History,path.toUtf8().toStdString()};
        auto one=host.attach(request,withImage,a),two=host.attach(request,withImage,b);host.active(one,true);host.active(two,true);
        await([&]{return a->publication.read()&&b->publication.read()&&a->publication.read()->artwork&&b->publication.read()->artwork;});
        check(a->publication.read()->artwork==b->publication.read()->artwork,"Artwork preparation not shared");
        const auto before=a->publication.read()->snapshot;
        check(a->publication.read()->artwork->image.pixelColor(0,0).alpha()==0,"Aspect ratio stretched");
        auto noFallback=withImage;noFallback.profileFallback=false;host.configure(one,noFallback,true,0);
        pump(100);check(!a->publication.read()->artwork&&b->publication.read()->artwork,"Consumer fallback policy leaked");
        check(host.sessions()==1&&a->publication.read()->snapshot==before,"Image policy restarted provider");
        host.configure(one,withImage,true,0);host.active(one,false);pump(50);check(!a->publication.read(),"Late hidden artwork published");
        host.active(one,true);await([&]{return a->publication.read()&&a->publication.read()->artwork;});
        withImage.normalPreset=3;host.configure(one,withImage,true,0);
        await([&]{return a->publication.read()&&a->publication.read()->normalFrame&&a->publication.read()->artwork;});pump(100);
        check(a->publication.read()->artwork&&host.animationScheduled(),"Animation dropped artwork");
        const auto outgoing=a->publication.read();
        write(path,QByteArray("21:15 : Artist - Artwork transition\r\n"));
        await([&]{auto frame=a->publication.read();return frame&&frame->normalSample.transitioning;});
        check(a->publication.read()->snapshot==outgoing->snapshot&&a->publication.read()->artwork==outgoing->artwork,"OUT dropped retained artwork/snapshot");
        await([&]{auto frame=a->publication.read();return frame&&frame->snapshot->track->title=="Artwork transition"&&!frame->normalSample.transitioning&&frame->artwork;});
        check(a->publication.read()->snapshot->track==a->publication.read()->normalFrame->track,"Artwork IN presentation incoherent");
        withImage.normalPreset=0;withImage.animationProfile=1;host.configure(one,withImage,true,0);
        await([&]{return a->publication.read()&&a->publication.read()->artwork&&host.animationScheduled();});
        const auto moving=a->publication.read();const auto rasters=host.visualStats().renders;
        pump(250);auto later=a->publication.read();
        check(later!=moving&&later->title==moving->title&&later->artwork==moving->artwork,"Multilayer rerasterized frames");
        check(later->layerMotion[1].x>moving->layerMotion[1].x&&later->layerMotion[3].scale>moving->layerMotion[3].scale,"Independent layers not advancing");
        check(host.visualStats().renders==rasters&&host.sessions()==1,"Animation changed CPU raster/session");
        check(b->publication.read()->layerMotion[1].x==0,"Motion policy leaked to peer");
        host.active(one,false);pump(50);check(!host.animationScheduled()&&!a->publication.read(),"Hidden multilayer timer alive");
        host.active(one,true);await([&]{return a->publication.read()&&a->publication.read()->artwork;});
        check(a->publication.read()->layerMotion[1].x>=later->layerMotion[1].x,"Resume lost paused phase");
        withImage.animationProfile=0;host.configure(one,withImage,true,0);pump(100);
        check(!host.animationScheduled()&&a->publication.read()->layerMotion[1].x==0,"Disable motion retained timer/transform");
        const auto basic=a->publication.read();
        write(path,QByteArray("21:16 : Artist - BASIC pending\r\n"));
        await([&]{auto frame=a->publication.read();return frame&&frame->normalSample.transitioning;});
        check(a->publication.read()->wholeTransition&&a->publication.read()->snapshot==basic->snapshot&&a->publication.read()->artwork==basic->artwork,"BASIC OUT did not retain coherent presentation");
        write(path,QByteArray("21:17 : Artist - BASIC latest\r\n"));
        await([&]{auto frame=a->publication.read();return frame&&frame->snapshot->track->title=="BASIC latest"&&!frame->normalSample.transitioning&&frame->artwork;});
        check(host.sessions()==1&&!host.animationScheduled(),"BASIC completed transition retained timer/session duplication");
        write(path,QByteArray("21:18 : Artist - Configure pending\r\n"));
        await([&]{auto frame=a->publication.read();return frame&&frame->normalSample.transitioning;});
        withImage.normalPreset=1;withImage.wholeTransition=true;host.configure(one,withImage,true,0);
        await([&]{return a->publication.read()&&a->publication.read()->artwork;});
        check(a->publication.read()->wholeTransition&&!a->publication.read()->normalSample.transitioning&&a->publication.read()->snapshot->track==a->publication.read()->normalFrame->track,"Reconfigure midway mixed snapshot and text");
        check(host.sessions()==1,"Reconfigure midway restarted reader");
        host.detach(one);host.detach(two);check(host.sessions()==0&&!a->publication.read()&&!b->publication.read(),"Artwork detach retained demand");
        }
        {
        obs::SessionHost host;auto pro=layout;pro.proPreset=1;pro.proLightIntensity=65;
        pro.artworkEnabled=true;pro.streamerProfile=core::ImageReference{core::ImageKind::LocalFile,temp.filePath("profile.png").toUtf8().toStdString(),"1"};
        const providers::Request request{"virtualdj",providers::Mode::History,path.toUtf8().toStdString()};
        const auto id=host.attach(request,pro,a);host.active(id,true);await([&]{return ready(a);});
        await([&]{return a->publication.read()&&a->publication.read()->artwork;});
        const auto original=a->publication.read();const auto rasters=host.visualStats().renders;
        pump(150);check(!host.animationScheduled()&&a->publication.read()==original,"Static PRO scheduled animation");
        pro.proLightIntensity=100;host.configure(id,pro,true,0);
        check(a->publication.read()->proArt==original->proArt&&a->publication.read()->lightGain==1&&host.visualStats().renders==rasters,"PRO intensity rasterized decoration");
        pro.proLightPulse=true;host.configure(id,pro,true,0);const auto initial=a->publication.read();pump(150);
        check(host.animationScheduled()&&a->publication.read()->lightGain!=initial->lightGain&&a->publication.read()->proArt==original->proArt,"PRO pulse lacks shared timer/cache");
        pro.proLightIntensity=0;host.configure(id,pro,true,0);check(!host.animationScheduled()&&a->publication.read()->lightGain==0,"Zero intensity retained pulse demand");
        check(host.sessions()==1&&a->publication.read()->snapshot==original->snapshot,"PRO controls restarted Core");
        pro.proHue=120;host.configure(id,pro,true,0);
        check(a->publication.read()&&a->publication.read()->artwork==original->artwork&&a->publication.read()->snapshot==original->snapshot,"Hue cleared visible frame or reloaded artwork");
        pump(100);check(a->publication.read()->artwork==original->artwork&&!host.animationScheduled(),"Hue introduced flicker/animation demand");
        const auto beforeDrag=host.visualStats().renders;QElapsedTimer dragTime;dragTime.start();
        for(int hue=0;hue<360;hue+=3){pro.proHue=hue;host.configure(id,pro,true,0);}
        const auto dragNanoseconds=dragTime.nsecsElapsed();
        check(host.visualStats().renders==beforeDrag&&a->publication.read()->artwork==original->artwork,"Hue drag rebuilt rasters/reloaded cover");
        std::cout<<"Hue CPU configure: 120 changes, total_us="<<dragNanoseconds/1000<<", mean_us="<<dragNanoseconds/120000<<"; no GPU timing\n";
        host.active(id,false);check(!a->publication.read()&&!host.animationScheduled()&&host.sessions()==0,"Hidden PRO retained work");host.detach(id);
        }
        {
            obs::SessionHost host;auto demoLayout=layout;core::TrackState fictitious{"Demo Unicode 日本語","Fictitious artist"};
            const auto id=host.attachDemo(fictitious,demoLayout,a);host.active(id,true);
            check(a->publication.read()&&a->publication.read()->snapshot->track->title==fictitious.title,"Fictitious preview did not publish");
            check(host.sessions()==0&&host.consumers()==1&&!host.scheduled()&&!host.animationScheduled(),"Demo opened provider/polling demand");
            check(a->publication.read()->snapshot->track->origin->provider=="demo"&&!a->publication.read()->snapshot->track->identity,"Demo pretends DJ/identity");
            const auto before=host.refreshes();pump(150);check(host.refreshes()==before,"Static demo polling");
            fictitious.title="Second fictitious track";host.demoTrack(id,fictitious);pump(1100);
            check(a->publication.read()->snapshot->track->title==fictitious.title,"Edited demo not published");
            demoLayout.animationProfile=1;demoLayout.animationSpeed=200;host.configure(id,demoLayout,true,0);pump(100);
            check(host.animationScheduled()&&!host.scheduled(),"Demo animation uses provider timer");
            const auto motionBefore=a->publication.read();const auto speedRasters=host.visualStats().renders;
            demoLayout.animationSpeed=50;host.configure(id,demoLayout,true,0);
            check(host.visualStats().renders==speedRasters&&a->publication.read()->title==motionBefore->title&&std::abs(a->publication.read()->layerMotion[1].y-motionBefore->layerMotion[1].y)<.5,"Speed change rerasterized/restarted animation phase");
            auto invalid=fictitious;invalid.title=std::string(5000,'x');bool rejected=false;try{host.demoTrack(id,invalid);}catch(const std::invalid_argument&){rejected=true;}
            check(rejected&&a->publication.read(),"Invalid demo disturbed valid preview");
            host.active(id,false);check(!a->publication.read()&&!host.animationScheduled()&&!host.scheduled()&&host.consumers()==0,"Hidden demo retained resources");host.detach(id);
        }
        {
            auto classic=layout;classic.normalPreset=1;classic.canvas.setWidth(720);classic.normalDuration=200;
            obs::SessionHost host;auto consumer=std::make_shared<obs::Consumer>();core::TrackState track{"Custom Classic","Artist"};const auto id=host.attachDemo(track,classic,consumer);host.active(id,true);
            await([&]{return consumer->publication.read()&&consumer->publication.read()->normalFrame;});
            const auto original=consumer->publication.read();classic.normalText[0].family=QStringLiteral("Arial");classic.normalText[0].pixelSize=36;classic.normalText[0].effects.outline=2;
            host.configure(id,classic,true,0);await([&]{return consumer->publication.read()->normalFrame!=original->normalFrame;});
            check(host.consumers()==1&&host.sessions()==0,"Classic style restarted reader or consumer");
            const auto styled=consumer->publication.read();check(styled->normalFrame->text[1][0].pixels->image!=original->normalFrame->text[1][0].pixels->image,"Classic configure ignored font/effect");
            track.title="Changed Custom Classic";host.demoTrack(id,track);await([&]{const auto frame=consumer->publication.read();return frame&&frame->snapshot->track->title==track.title&&!frame->normalSample.transitioning;});
            const auto expected=visual::TrackVisualHost{}.prepareNormal(visual::normal::presetFromSetting(1),consumer->publication.read()->snapshot->track,720,classic.normalGap,classic.normalText);
            check(consumer->publication.read()->normalFrame->text[1][0].pixels->image==expected->text[1][0].pixels->image,"Transition lost classic customization");
            host.active(id,false);host.detach(id);check(!host.animationScheduled()&&host.consumers()==0,"Classic customization retained demand");
        }
        {
            obs::SessionHost host;auto eco=layout;eco.performanceProfile=0;auto maximum=layout;maximum.performanceProfile=2;
            auto low=std::make_shared<obs::Consumer>(),high=std::make_shared<obs::Consumer>();const providers::Request request{"virtualdj",providers::Mode::History,path.toUtf8().toStdString()};
            const auto one=host.attach(request,eco,low),two=host.attach(request,maximum,high);host.active(one,true);await([&]{return low->publication.read()!=nullptr;});
            check(host.pollingIntervalMs()==250&&host.animationIntervalMs()==33,"Eco cadence/inactive Maximum wrong");
            host.active(two,true);await([&]{return high->publication.read()!=nullptr;});check(host.sessions()==1&&host.pollingIntervalMs()==50&&host.animationIntervalMs()==16,"Active demand not shared at highest profile");
            host.active(two,false);check(host.pollingIntervalMs()==250&&host.animationIntervalMs()==33,"Hidden Maximum retained demand");
            const auto before=low->publication.read();const auto renders=host.visualStats().renders;eco.performanceProfile=1;host.configure(one,eco,true,0);
            check(host.pollingIntervalMs()==100&&host.animationIntervalMs()==16&&host.sessions()==1,"Balanced changed session or cadence");check(low->publication.read()==before&&host.visualStats().renders==renders,"Performance profile rebuilt/cleared visual frame");
            auto invalid=eco;invalid.performanceProfile=3;bool rejected=false;try{host.configure(one,invalid,true,0);}catch(const std::invalid_argument&){rejected=true;}check(rejected&&host.pollingIntervalMs()==100,"Invalid performance profile mutated host");
            host.active(one,false);host.detach(one);host.detach(two);check(!host.scheduled()&&!host.animationScheduled()&&host.consumers()==0,"Profiles introduced idle demand");
            auto demoLayout=layout;demoLayout.proPreset=2;demoLayout.performanceProfile=0;auto preview=std::make_shared<obs::Consumer>();const auto demo=host.attachDemo({"Record","Artist"},demoLayout,preview);host.active(demo,true);await([&]{return preview->publication.read()!=nullptr;});
            check(!host.scheduled()&&host.animationScheduled()&&host.animationIntervalMs()==33,"Eco demo polling/cadence wrong");const auto visual=preview->publication.read();demoLayout.performanceProfile=2;host.configure(demo,demoLayout,true,0);check(host.animationIntervalMs()==16&&preview->publication.read()==visual,"Maximum demo reset phase/frame");host.active(demo,false);host.detach(demo);
        }
        {
            obs::SessionHost host;auto staticLayout=layout;staticLayout.proPreset=1;
            auto movingLayout=layout;movingLayout.proPreset=2;movingLayout.proLightIntensity=80;movingLayout.proLightPulse=true;movingLayout.animationProfile=1;
            auto still=std::make_shared<obs::Consumer>(),moving=std::make_shared<obs::Consumer>();
            const auto one=host.attachDemo({"Static","Artist"},staticLayout,still),two=host.attachDemo({"Vinyl","Artist"},movingLayout,moving);
            host.active(one,true);host.active(two,true);const auto original=still->publication.read();const auto before=host.schedulingStats();const auto rasters=host.visualStats().renders;
            pump(180);const auto after=host.schedulingStats();const auto passes=after.animationPasses-before.animationPasses;
            check(passes>2&&after.animationVisits-before.animationVisits==passes,"Animation visited static consumer");
            check(after.staticSkips-before.staticSkips==passes&&still->publication.read()==original,"Static frame changed during another source animation");
            check(after.dynamicCopies-before.dynamicCopies<=passes,"Dynamic motion/light copied more than once per tick");
            check(host.visualStats().renders==rasters&&!host.scheduled(),"Animation optimization introduced raster/polling");
            const auto frame=moving->publication.read();const double elapsed=frame->recordAngle/.06;
            check(std::abs(frame->recordShimmer-(.06+.025*std::sin(elapsed*.0062831853)+.01*std::sin(elapsed*.0157079633)))<1e-12,"Vinyl shimmer changed");
            check(std::abs(frame->lightGain-.8*(.65+.35*visual::animation::oscillation(elapsed,4000)))<1e-12,"Pulse changed relative to rotation");
            const auto expected=visual::animation::program(1).sample(elapsed);
            check(std::equal(expected.begin(),expected.end(),frame->layerMotion.begin(),visual::animation::same),"Coalescing desynchronized multilayer clock");
            host.configure(one,staticLayout,true,70);await([&]{return blank(still);});
            check(moving->publication.read()&&host.animationScheduled(),"Static expiry lost while animation running");
            host.configure(one,staticLayout,true,0);await([&]{return ready(still);});
            host.demoTrack(one,{"Static changed","Artist"});await([&]{const auto current=still->publication.read();return current&&current->snapshot->track->title=="Static changed"&&!current->normalSample.transitioning;});
            check(host.animationScheduled()&&moving->publication.read(),"Static transition disturbed animated source");
            host.active(two,false);check(!moving->publication.read(),"Hidden animated source retained frame");
            pump(100);check(!host.animationScheduled()&&still->publication.read(),"Last animated consumer retained timer for static source");
            const auto stopped=host.schedulingStats();pump(80);check(host.schedulingStats().animationPasses==stopped.animationPasses,"Static-only timer still running");
            host.detach(one);host.detach(two);check(host.consumers()==0&&!host.scheduled()&&!host.animationScheduled(),"Optimized host retained idle demand");
        }
        std::cout<<"PASS "<<checks<<" native source session host checks\n";return 0;
    }catch(const std::exception &error){std::cerr<<"FAIL "<<checks<<": "<<error.what()<<'\n';return 1;}
}

