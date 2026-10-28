// SPDX-License-Identifier: MIT
#include "playing/companion/transport.hpp"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QSslSocket>
#include <QTcpSocket>
#include <QThread>
#include <iostream>
#include <stdexcept>
using namespace playing::core;
using namespace playing::providers;
using namespace playing::companion;
namespace net=playing::providers::network;
namespace {
unsigned checks=0;
void check(bool condition,const char *reason) {++checks; if(!condition) throw std::runtime_error(reason);}
bool wait(const std::function<bool()> &condition,int timeout=5000) {
    QElapsedTimer timer; timer.start();
    while(!condition() && timer.elapsed()<timeout) {QCoreApplication::processEvents(QEventLoop::AllEvents,10); QThread::msleep(1);}
    QCoreApplication::processEvents(); return condition();
}
QByteArray file(const QString &path) {QFile f(path); if(!f.open(QIODevice::ReadOnly)) throw std::runtime_error("Missing ephemeral TLS fixture"); return f.readAll();}
TrackState song(const char *id="1") {
    TrackState t{u8"Canción 🎵",u8"Artista"}; t.identity=TrackIdentity{"DJ-library",id}; t.origin=TrackOrigin{"serato","automatic"}; return t;
}
void transport(const ServerIdentity &identity) {
    Sender sender; const auto credential=newDeviceCredential();
    check(credential.id.size()==32 && credential.secret.size()==32,"Credential bounds");
    check(sender.pair(credential),"Pair credential"); check(!sender.pair(credential),"Duplicate pairing");
    check(!sender.pair({"invalid","short"}),"Invalid credential");
    check(!sender.listen({},"127.0.0.1",0),"Invalid identity");
    check(sender.listen(identity,"127.0.0.1",0) && sender.port()!=0,"TLS listener failed");
    const ReceiverSettings settings{"127.0.0.1",sender.port(),"OBS Playing Companion",certificatePin(identity.certificate),credential};
    PlayingCore dj; dj.start(); dj.observe({Playback::Playing,song()}); sender.publish(dj.snapshot());
    ProviderRegistry registry; auto channel=std::make_shared<net::Channel>();
    check(net::registerProvider(registry,channel)==Registration::Registered,"Network registration");
    const Request request{"network",Mode::Automatic,""}; const auto a=registry.acquire(request),b=registry.acquire(request);
    Receiver receiver(channel); receiver.changed=[&] {registry.refresh(request);};
    auto rejected=settings; rejected.certificateSha256=QByteArray(32,'x');
    check(receiver.connectTo(rejected),"Begin wrong-pin probe");
    check(wait([&] {return !receiver.lastError().isEmpty();}),"Wrong pin was not rejected");
    check(!receiver.connected() && sender.clientCount()==0 && !registry.snapshot(a.token)->track,"Wrong pin published state");
    rejected=settings; rejected.device.secret[0]=static_cast<char>(rejected.device.secret[0]^1);
    check(receiver.connectTo(rejected),"Begin wrong-token probe");
    check(wait([&] {return !receiver.lastError().isEmpty();}),"Wrong secret was not rejected");
    check(!receiver.connected() && !registry.snapshot(a.token)->track,"Wrong secret published state");
    rejected=settings; rejected.certificateName="wrong-certificate-name";
    check(receiver.connectTo(rejected),"Begin wrong-name probe");
    check(wait([&] {return !receiver.lastError().isEmpty();}),"Wrong name was not rejected");
    check(!receiver.connected(),"Pin bypassed name verification");
    check(receiver.connectTo(settings),"Begin encrypted connection");
    const auto connected=wait([&] {return receiver.connected() && registry.snapshot(a.token)->metadata.has_value();});
    if(!connected) std::cerr<<"TLS fixture error: "<<receiver.lastError().toStdString()<<'\n';
    check(connected,"Authenticated TLS state not received");
    check(sender.clientCount()==1,"Authenticated count");
    auto first=registry.snapshot(a.token); check(first==registry.snapshot(b.token),"Consumers did not share");
    check(first->track->title==song().title && first->track->identity==song().identity && first->track->origin==song().origin,"Remote Unicode/identity/origin changed");
    check(first->trackRevision!=dj.snapshot()->trackRevision,"Remote imported local token");
    sender.publish(dj.snapshot()); QCoreApplication::processEvents(); check(registry.snapshot(a.token)==first,"Duplicate snapshot published");
    {
        auto otherChannel=std::make_shared<net::Channel>(); Receiver duplicate(otherChannel);
        check(duplicate.connectTo(settings),"Begin duplicate device");
        check(wait([&] {return !duplicate.lastError().isEmpty();}),"Duplicate active device not rejected");
        check(receiver.connected() && sender.clientCount()==1,"Duplicate damaged original client");
    }
    dj.observe({Playback::Playing,song("2")}); sender.publish(dj.snapshot());
    check(wait([&] {return registry.snapshot(a.token)->track && registry.snapshot(a.token)->track->identity->value=="2";}),"Track switch missing");
    check(first->track->identity->value=="1","Retained snapshot mutated");
    dj.observe({Playback::Paused,std::nullopt}); sender.publish(dj.snapshot());
    check(wait([&] {return registry.snapshot(a.token)->playback==Playback::Paused;}),"Remote pause missing");
    check(!registry.snapshot(a.token)->metadata && registry.snapshot(a.token)->track->identity->value=="2","Paused track lost");
    receiver.stop(); check(!registry.snapshot(a.token)->track,"Stop retained remote track");
    check(wait([&] {return sender.clientCount()==0;}),"Stop did not release client");
    check(receiver.connectTo(settings),"Reconnect paused snapshot");
    check(wait([&] {return receiver.connected() && registry.snapshot(a.token)->playback==Playback::Paused;}),"Initial paused snapshot failed");
    check(registry.snapshot(a.token)->track->identity->value=="2","Reconnect paused track missing");
    auto invalidSettings=settings; invalidSettings.port=0;
    check(!receiver.connectTo(invalidSettings),"Invalid settings accepted");
    check(!registry.snapshot(a.token)->track && !receiver.lastError().isEmpty(),"Failed reconnect retained old song");
    check(wait([&] {return sender.clientCount()==0;}),"Invalid reconnect kept client");
    check(receiver.connectTo(settings),"Reconnect after invalid settings");
    check(wait([&] {return receiver.connected() && registry.snapshot(a.token)->track!=nullptr;}),"Recovery after invalid settings");
    auto local=song("3"); local.artwork={ArtworkStatus::Ready,ImageReference{ImageKind::LocalFile,"C:/DJ-PC/cover.png","1"}};
    dj.observe({Playback::Playing,local}); sender.publish(dj.snapshot());
    check(wait([&] {return registry.snapshot(a.token)->track && registry.snapshot(a.token)->track->identity->value=="3";}),"Image snapshot missing");
    check(registry.snapshot(a.token)->track->artwork.status==ArtworkStatus::Missing,"Remote local image was used");
    check(resolveArtwork(registry.snapshot(a.token)->track->artwork,ImageReference{ImageKind::Asset,"streamer-profile",""})->location=="streamer-profile","Profile fallback missing");
    const auto secondCredential=newDeviceCredential(); check(sender.pair(secondCredential),"Pair second device");
    {
        auto otherChannel=std::make_shared<net::Channel>(); Receiver second(otherChannel);
        auto otherSettings=settings; otherSettings.device=secondCredential;
        check(second.connectTo(otherSettings),"Begin second device");
        check(wait([&] {return second.connected() && sender.clientCount()==2;}),"Two clients not connected");
        check(sender.revoke(secondCredential.id),"Revoke device");
        check(wait([&] {return !second.connected() && sender.clientCount()==1;}),"Revocation did not disconnect");
        check(!sender.revoke(secondCredential.id),"Double revoke");
    }
    // Backpressure: do not pump the event loop while publishing a burst.
    for(unsigned i=0;i<100;++i) {
        auto large=song(); large.title=std::string(4096,'x'); large.identity->value=std::to_string(i);
        dj.observe({Playback::Playing,large}); sender.publish(dj.snapshot());
    }
    check(wait([&] {return !receiver.connected() && sender.clientCount()==0;}),"Unbounded slow-client queue");
    sender.stop(); check(sender.port()==0 && sender.clientCount()==0,"Sender stop");
    registry.release(a.token); registry.release(b.token); check(registry.consumerCount()==0,"Registry release");
}
void artwork(const ServerIdentity &identity,const QString &fixture) {
    QTemporaryDir files(fixture+"/artwork-XXXXXX");check(files.isValid(),"Image fixture root");
    const auto source=files.path()+QString::fromUtf8(u8"/portada-á.png");
    QImage pixels(300,220,QImage::Format_ARGB32);pixels.fill(QColor(240,20,30,180));check(pixels.save(source,"PNG"),"Source cover fixture");
    auto songWithCover=song("art-1");songWithCover.artwork={ArtworkStatus::Ready,ImageReference{ImageKind::LocalFile,source.toUtf8().toStdString(),"one"}};
    Sender sender;const auto credential=newDeviceCredential();check(sender.pair(credential)&&sender.listen(identity,"127.0.0.1",0),"Artwork sender");
    PlayingCore dj;dj.start();dj.observe({Playback::Playing,songWithCover});sender.publish(dj.snapshot());
    ProviderRegistry registry;auto channel=std::make_shared<net::Channel>();check(net::registerProvider(registry,channel)==Registration::Registered,"Image channel");
    const Request request{"network",Mode::Automatic,""};const auto consumer=registry.acquire(request);
    Receiver receiver(channel,nullptr,files.path()+"/receiver-cache");receiver.changed=[&]{registry.refresh(request);};
    const ReceiverSettings settings{"127.0.0.1",sender.port(),"OBS Playing Companion",certificatePin(identity.certificate),credential};
    const auto ready=[&]{const auto snapshot=registry.snapshot(consumer.token);return snapshot->track&&snapshot->track->artwork.status==ArtworkStatus::Ready;};
    check(receiver.connectTo(settings),"Image receiver connect");check(wait(ready),"Local cover did not cross authenticated transport");
    const auto first=*registry.snapshot(consumer.token)->track->artwork.image;
    check(first.location!=songWithCover.artwork.image->location&&QString::fromStdString(first.location).startsWith(files.path()+"/receiver-cache/"),"Remote path used instead of private cache");
    const QImage received(QString::fromUtf8(first.location));check(!received.isNull()&&received.size()==pixels.size()&&received.pixelColor(100,100)==pixels.pixelColor(100,100),"Cover pixels or alpha changed");
    receiver.stop();check(!registry.snapshot(consumer.token)->track,"Image stop retained song");
    check(receiver.connectTo(settings)&&wait(ready),"Image reconnect");check(registry.snapshot(consumer.token)->track->artwork.image->version==first.version,"Content cache changed on reconnect");
    pixels.fill(QColor(10,90,230));check(pixels.save(source,"PNG"),"Changed picture fixture");songWithCover.identity->value="art-2";songWithCover.title="Second cover";songWithCover.artwork.image->version="two";
    dj.observe({Playback::Playing,songWithCover});sender.publish(dj.snapshot());
    check(wait([&]{return ready()&&registry.snapshot(consumer.token)->track->title=="Second cover"&&registry.snapshot(consumer.token)->track->artwork.image->version!=first.version;}),"Changed song cover missing");
    check(QImage(QString::fromUtf8(registry.snapshot(consumer.token)->track->artwork.image->location)).pixelColor(100,100)==pixels.pixelColor(100,100),"Previous cover applied to next song");
    // An older client authenticates unchanged and receives PNX1 only, with no
    // unsolicited image packets or DJ-PC file paths.
    const auto legacyCredential=newDeviceCredential();check(sender.pair(legacyCredential),"Legacy credential");
    QSslSocket legacy;legacy.setPeerVerifyMode(QSslSocket::VerifyNone);legacy.connectToHostEncrypted("127.0.0.1",sender.port());
    check(wait([&]{return legacy.isEncrypted();}),"Legacy TLS");
    const QByteArray auth="NPA1"+legacyCredential.id+legacyCredential.secret;
    QByteArray authFrame(4,'\0');authFrame[3]=char(auth.size());authFrame+=auth;legacy.write(authFrame);
    check(wait([&]{return sender.clientCount()==2&&legacy.bytesAvailable()>40;}),"Older receiver authentication");
    auto legacyBytes=legacy.readAll();check(legacyBytes.mid(4,4)=="NPW1"&&legacyBytes.mid(8,4)=="ART1","Backward-compatible welcome");
    legacyBytes.remove(0,40);check(legacyBytes.mid(4,4)=="PNX1","Legacy metadata frame");
    const auto legacyState=net::decode(std::string_view(legacyBytes.constData(),std::size_t(legacyBytes.size())));
    check(legacyState&&legacyState->observation.metadata&&legacyState->observation.metadata->artwork.status==ArtworkStatus::Missing,"Legacy receiver got nonportable artwork");
    legacy.abort();
    dj.observe({Playback::Paused,std::nullopt});sender.publish(dj.snapshot());check(wait([&]{return ready()&&registry.snapshot(consumer.token)->playback==Playback::Paused;}),"Paused retained cover transfer");
    auto invalid=songWithCover;invalid.identity->value="broken";invalid.artwork.image->location=files.path().toStdString()+"/missing.png";
    dj.observe({Playback::Playing,invalid});sender.publish(dj.snapshot());check(wait([&]{const auto s=registry.snapshot(consumer.token);return s->track&&s->track->identity->value=="broken";}),"Missing image blocked text");
    check(registry.snapshot(consumer.token)->track->artwork.status!=ArtworkStatus::Ready,"Missing file reused previous cover");
    dj.observe({Playback::Playing,songWithCover});sender.publish(dj.snapshot());dj.observe({Playback::Waiting,std::nullopt});sender.publish(dj.snapshot());
    check(wait([&]{return !registry.snapshot(consumer.token)->track;}),"Waiting state retained image");
    QElapsedTimer settle;settle.start();while(settle.elapsed()<150){QCoreApplication::processEvents();QThread::msleep(1);}
    check(!registry.snapshot(consumer.token)->track,"Late cover restored cleared song");
    receiver.stop();sender.stop();registry.release(consumer.token);
}
}
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    try {
        check(argc==2,"Fixture directory required");
        check(QSslSocket::setActiveBackend("schannel"),"Schannel unavailable");
        check(QSslSocket::supportsSsl(),"TLS unavailable");
        const auto dir=QString::fromLocal8Bit(argv[1]);
        const ServerIdentity identity{QSslCertificate(file(dir+"/certificate.pem")),QSslKey(file(dir+"/private-key.pem"),QSsl::Rsa)};
        check(!identity.certificate.isNull() && !identity.privateKey.isNull(),"Identity parse failed");
        transport(identity);
        artwork(identity,dir);
        std::cout<<checks<<" Companion TLS checks; Schannel sockets, certificate pin/name, device credentials, shared state, pause, profile fallback, revoke and backpressure; loopback only\n"; return 0;
    } catch(const std::exception &error) {std::cerr<<"FAIL: "<<error.what()<<'\n'; return 1;}
}
