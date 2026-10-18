// SPDX-License-Identifier: MIT
#include "playing/companion/profile.hpp"
#include "playing/companion/transport.hpp"
#include <QCoreApplication>
#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QImage>
#include <QFileInfo>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QThread>
#include <iostream>
#include <stdexcept>
using namespace playing;
namespace {
unsigned checks=0;
void check(bool ok,const char *message){++checks;if(!ok)throw std::runtime_error(message);}
bool wait(const std::function<bool()> &predicate){QElapsedTimer clock;clock.start();while(!predicate()&&clock.elapsed()<4000){QCoreApplication::processEvents();QThread::msleep(1);}return predicate();}
void settle(){QElapsedTimer clock;clock.start();while(clock.elapsed()<200){QCoreApplication::processEvents();QThread::msleep(1);}}
QByteArray packet(QByteArray body){QByteArray bytes(4,'\0');for(int i=0;i<4;++i)bytes[i]=char((quint32(body.size())>>((3-i)*8))&255);return bytes+body;}
class Listener:public QTcpServer{public:std::function<void(qintptr)> accept;void incomingConnection(qintptr descriptor) override{accept(descriptor);}};
struct Fixture {
    companion::ServerIdentity identity=companion::createIdentity();
    companion::DeviceCredential credential=companion::newDeviceCredential();
    Listener server;QSslSocket *socket=nullptr;QByteArray buffer,epoch;bool authenticated=false,capability=false;
    explicit Fixture(bool modern):epoch(modern?QByteArray("ART1")+QByteArray(28,'x'):QByteArray(32,'a')){
        server.accept=[this](qintptr descriptor){
            socket=new QSslSocket(&server);check(socket->setSocketDescriptor(descriptor),"Wire socket descriptor");
            auto config=QSslConfiguration::defaultConfiguration();config.setLocalCertificate(identity.certificate);config.setPrivateKey(identity.privateKey);config.setPeerVerifyMode(QSslSocket::VerifyNone);socket->setSslConfiguration(config);
            QObject::connect(socket,&QSslSocket::readyRead,&server,[this]{
                buffer+=socket->readAll();
                if(!authenticated){if(buffer.size()<72)return;check(buffer==packet("NPA1"+credential.id+credential.secret),"Wire authentication unchanged");buffer.clear();authenticated=true;socket->write(packet("NPW1"+epoch));state(1,"First");}
                else if(buffer.size()>=8){check(buffer==packet("NPC1"),"Unexpected client capability");capability=true;buffer.clear();}
            });socket->startServerEncryption();
        };check(server.listen(QHostAddress::LocalHost,0),"Wire listener");
    }
    companion::ReceiverSettings settings()const{return {"127.0.0.1",server.serverPort(),"OBS Playing Companion",companion::certificatePin(identity.certificate),credential};}
    void state(std::uint64_t sequence,const std::string &title){
        core::TrackState track{title,"Artist"};track.artwork={core::ArtworkStatus::Missing,{}};
        providers::network::Message message{epoch.toStdString(),sequence,providers::network::MessageKind::Snapshot,{core::Playback::Playing,track}};
        socket->write(QByteArray::fromStdString(*providers::network::encode(message)));
    }
    void image(std::uint64_t sequence,const QByteArray &bytes,bool corrupt=false,QByteArray stream={}){
        if(stream.isEmpty())stream=epoch;QByteArray body="NPI1"+stream;for(int i=7;i>=0;--i)body+=char((sequence>>(i*8))&255);
        body+=(corrupt?QByteArray(32,'!'):QCryptographicHash::hash(bytes,QCryptographicHash::Sha256));body+=bytes;socket->write(packet(body));
    }
};
QByteArray png(int edge,QRgb color){QImage image(edge,edge,QImage::Format_ARGB32);image.fill(color);QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);check(image.save(&buffer,"PNG"),"Wire image fixture");return bytes;}
void test(bool modern){
    QTemporaryDir directory(QDir::currentPath()+"/image-wire-XXXXXX");check(directory.isValid(),"Wire private cache");
    Fixture peer(modern);auto channel=std::make_shared<providers::network::Channel>();
    companion::Receiver receiver(channel,nullptr,directory.path()+"/cache");
    std::uint64_t cursor=0;providers::Sample last;
    receiver.changed=[&]{auto sample=channel->sample(cursor);if(sample.kind!=providers::SampleKind::NoChange)last=std::move(sample);};
    check(receiver.connectTo(peer.settings()),"Wire connect");
    check(wait([&]{return receiver.connected()&&last.observation&&last.observation->metadata.has_value();}),"Legacy-compatible metadata");
    if(!modern){settle();check(!peer.capability,"New receiver sent extension to old sender");check(last.observation->metadata->title=="First","Old sender title lost");receiver.stop();return;}
    check(wait([&]{return peer.capability;}),"Explicit artwork capability missing");
    const auto red=png(16,qRgba(255,0,0,150)),blue=png(16,qRgb(0,0,255));
    peer.image(1,red,true);settle();check(receiver.connected()&&last.observation->metadata->artwork.status==core::ArtworkStatus::Missing,"Corrupt digest changed artwork or text connection");
    check(!QFileInfo::exists(directory.path()+"/cache"),"Corrupt payload reached disk");
    peer.image(1,png(600,qRgb(0,1,2)));settle();check(!QFileInfo::exists(directory.path()+"/cache"),"Oversized image decoded/cached");
    peer.image(1,red);check(wait([&]{return last.observation&&last.observation->metadata&&last.observation->metadata->artwork.status==core::ArtworkStatus::Ready;}),"Valid wire image missing");
    const auto first=*last.observation->metadata->artwork.image;
    check(QImage(QString::fromUtf8(first.location)).pixelColor(0,0)==QColor(255,0,0,150),"Wire alpha lost");
    peer.state(2,"Next");peer.image(1,blue);check(wait([&]{return last.observation&&last.observation->metadata&&last.observation->metadata->title=="Next";}),"New text blocked by stale image");
    settle();check(last.observation->metadata->artwork.status==core::ArtworkStatus::Missing,"Stale image applied to next song");
    peer.image(2,blue,false,QByteArray(32,'b'));settle();check(last.observation->metadata->artwork.status==core::ArtworkStatus::Missing,"Wrong epoch image accepted");
    providers::network::Message heartbeat{peer.epoch.toStdString(),3,providers::network::MessageKind::Heartbeat,{}};
    peer.socket->write(QByteArray::fromStdString(*providers::network::encode(heartbeat)));peer.image(2,blue);
    check(wait([&]{return last.observation->metadata->artwork.status==core::ArtworkStatus::Ready;}),"Heartbeat invalidated late current image");
    check(last.observation->metadata->artwork.image->version!=first.version,"Cover cache reused wrong content");
    peer.image(2,QByteArray(60*1024+1,'x'));check(wait([&]{return !receiver.connected();}),"Oversized transfer accepted");
    check(receiver.lastFailure()==companion::ConnectionFailure::Protocol,"Oversized transfer failure type");
    receiver.stop();
}
}
int main(int argc,char **argv){QCoreApplication app(argc,argv);try{check(QSslSocket::setActiveBackend("schannel"),"Wire TLS backend");test(false);test(true);std::cout<<checks<<" artwork wire checks; legacy sender, negotiated capability, alpha, digest/dimension/size bounds, stale sequence and epoch\n";return 0;}catch(const std::exception &error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}
