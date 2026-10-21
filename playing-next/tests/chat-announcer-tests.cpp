// SPDX-License-Identifier: MIT
#include "../src/obs/chat-announcer.hpp"
#include "playing/companion/protected-store.hpp"
#include "playing/ui/i18n.hpp"
#include <QApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QFile>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QThread>
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QDir>
#include <iostream>
#include <stdexcept>
using namespace playing;
namespace {
int checks=0;
void check(bool value,const char *why){++checks;if(!value)throw std::runtime_error(why);}
void pump(int ms){QElapsedTimer t;t.start();while(t.elapsed()<ms){QApplication::processEvents();QThread::msleep(2);}}
template<class F>void await(F f){QElapsedTimer t;t.start();while(!f()){pump(5);if(t.elapsed()>6000)throw std::runtime_error("Chat fixture timeout");}}
core::SnapshotPtr snapshot(QString title,QString artist,bool demo=false){auto s=std::make_shared<core::Snapshot>();s->running=true;s->playback=core::Playback::Playing;auto track=std::make_shared<core::TrackState>();track->title=title.toUtf8().toStdString();track->artist=artist.toUtf8().toStdString();if(demo)track->origin=core::TrackOrigin{"demo","local"};s->track=track;return s;}
struct Server:QObject{
    QTcpServer listener;int devices=0,polls=0,validations=0,sends=0,refreshes=0;bool scope=true,rejectValidation=false,sent=true,wrongClient=false,wrongUrl=false,pendingOnce=true;QJsonObject lastMessage;QByteArray lastScope;
    Server(){check(listener.listen(QHostAddress::LocalHost,0),"Local fixture listener");QObject::connect(&listener,&QTcpServer::newConnection,this,[this]{while(listener.hasPendingConnections()){
        auto *socket=listener.nextPendingConnection();auto bytes=std::make_shared<QByteArray>();QObject::connect(socket,&QTcpSocket::readyRead,this,[this,socket,bytes]{
            *bytes+=socket->readAll();const auto split=bytes->indexOf("\r\n\r\n");if(split<0)return;int length=0;for(const auto &line:bytes->left(split).split('\n'))if(line.toLower().startsWith("content-length:"))length=line.mid(15).trimmed().toInt();if(bytes->size()<split+4+length)return;
            const auto path=bytes->split(' ').value(1),body=bytes->mid(split+4,length);int status=200;QJsonObject result;
            if(path=="/device"){++devices;lastScope=body;result={{"device_code","fake-device"},{"user_code","ABCD-EFGH"},{"verification_uri",wrongUrl?"https://evil.example/activate":"https://www.twitch.tv/activate?public=true"},{"expires_in",600},{"interval",1}};}
            else if(path=="/token"){
                if(body.contains("grant_type=refresh_token")){++refreshes;rejectValidation=false;result={{"access_token","rotated-access"},{"refresh_token","rotated-refresh"}};}
                else{++polls;if(pendingOnce&&polls==1){status=400;result={{"message","authorization_pending"}};}else result={{"access_token","fake-access"},{"refresh_token","fake-refresh"}};}
            }else if(path=="/validate"){++validations;if(rejectValidation){status=401;result={{"status",401}};}else result={{"client_id",wrongClient?"other-app":"xdxao5ubz0csvj28n0s70b7k3g0lnx"},{"user_id","1234"},{"login","streamer"},{"scopes",scope?QJsonArray{"user:write:chat"}:QJsonArray{}},{"expires_in",3600}};}
            else if(path=="/messages"){++sends;lastMessage=QJsonDocument::fromJson(body).object();result={{"data",QJsonArray{QJsonObject{{"is_sent",sent},{"message_id","fixture"}}}}};}
            else{status=404;}
            const auto payload=QJsonDocument(result).toJson(QJsonDocument::Compact);socket->write("HTTP/1.1 "+QByteArray::number(status)+" Fixture\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: "+QByteArray::number(payload.size())+"\r\n\r\n"+payload);socket->disconnectFromHost();QObject::disconnect(socket,&QTcpSocket::readyRead,this,nullptr);
        });QObject::connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
    }});}
    obs::ChatAnnouncer::Endpoints endpoints()const{const auto base="http://127.0.0.1:"+QString::number(listener.serverPort());return {QUrl(base+"/"),QUrl(base+"/messages")};}
};
void grant(const QString &path){companion::ProtectedStore(path).save(QJsonDocument(QJsonObject{{"kind","playing-chat-v1"},{"client","xdxao5ubz0csvj28n0s70b7k3g0lnx"},{"access","fake-access"},{"refresh","fake-refresh"}}).toJson());}
}
int main(int argc,char **argv){QApplication app(argc,argv);app.setQuitOnLastWindowClosed(false);try{
    using Policy=obs::ChatPolicy;Policy p;Policy::Input a{"Track A","Artist A","Now: {artist} — {title}",3000};
    p.put(1,a);check(!p.poll(0),"Offline announcement");p.live(true);check(!p.poll(0)&&!p.poll(2999),"No settling delay");p.put(2,a);check(p.poll(3000)=="Now: Artist A — Track A","Duplicate sources did not coalesce");check(!p.pending()&&!p.poll(30000),"Repeated track announced");
    auto b=a;b.title="Track B";p.put(1,b);check(!p.pending()&&!p.poll(31000),"Conflicting sources selected arbitrarily");p.put(2,b);check(!p.poll(32000)&&p.poll(35000).has_value(),"Resolved song did not settle");p.put(1,{});p.put(2,{});check(!p.poll(40000),"Empty track announced");p.put(1,b);check(!p.pending(),"Pause/resume repeated announcement");
    p.put(1,a);check(!p.poll(36000)&&!p.poll(39000)&&p.poll(45000).has_value(),"Minimum send spacing");p.live(false);check(!p.poll(60000),"Stream stop ignored");p.live(true);check(!p.poll(60000)&&p.poll(63000).has_value(),"New stream did not reset dedupe");
    p.put(1,b);check(!p.poll(70000),"New song sent immediately");p.put(1,{});p.put(1,b);check(!p.poll(80000)&&!p.poll(82999),"Pause retained old settling deadline");check(p.poll(83000).has_value(),"Resumed new song never sent");
    auto literal=a;literal.title="Track {artist}";check(Policy::message(literal).contains("Track {artist}"),"Recursive substitution of metadata");literal.format="{artist}";check(Policy::message(literal).isEmpty(),"Missing title placeholder accepted");literal.format="/ban {artist} {title}";check(Policy::message(literal).isEmpty(),"Command template accepted");literal=a;literal.title=QString::fromUtf8("🎵").repeated(600);check(Policy::message(literal).toUcs4().size()==500,"Unicode message exceeds Twitch limit");
    Policy bounce;bounce.live(true);bounce.put(1,a);bounce.poll(0);check(bounce.poll(3000).has_value(),"Bounce baseline");bounce.put(1,b);bounce.poll(4000);bounce.put(1,a);bounce.put(1,b);check(!bounce.poll(15000)&&!bounce.poll(17999)&&bounce.poll(18000).has_value(),"Returning to a transient song reused an obsolete delay");
    QTemporaryDir temp;check(temp.isValid(),"Private fixture store");Server server;const auto path=temp.filePath("chat.pcs");
    {
        obs::ChatAnnouncer chat(path,server.endpoints());pump(50);check(server.devices==0&&server.validations==0&&!chat.scheduled(),"Idle contacted Twitch or started announcement timer");
        chat.configure(1,true,a.format,1000);chat.authorize();await([&]{return !chat.code().isEmpty();});check(chat.verification().host()=="www.twitch.tv","Wrong authorization destination");check(QUrlQuery(QString::fromUtf8(server.lastScope)).queryItemValue("scopes")=="user:write:chat","Excess OAuth scopes");await([&]{return chat.authorized();});check(server.polls==2,"Pending authorization mishandled");
        QFile protectedFile(path);check(protectedFile.open(QIODevice::ReadOnly),"No persisted grant");const auto protectedBytes=protectedFile.readAll();protectedFile.close();check(!protectedBytes.contains("fake-access")&&!protectedBytes.contains("fake-refresh"),"Plaintext credential on disk");
        auto d=chat.dialog(nullptr);check(d->findChild<QLabel*>("chatStatus")!=nullptr,"No authorization/status UI");d->close();pump(5);
        chat.observe(1,snapshot("Real Track","Real Artist"));pump(1200);check(server.sends==0,"Sent while offline");
        chat.broadcast(true,"different-account");pump(1200);check(server.sends==0,"Sent to mismatched broadcast account");
        chat.broadcast(true,"streamer");chat.observe(1,snapshot("Fiction","Preview",true));pump(1200);check(server.sends==0,"Fictitious track announced");
        chat.configure(2,true,a.format,1000);chat.observe(1,snapshot("Remote Track","Remote Artist"));chat.observe(2,snapshot("Remote Track","Remote Artist"));await([&]{return server.sends==1;});check(server.lastMessage.value("message").toString()=="Now: Remote Artist — Remote Track","Remote metadata lost");check(server.lastMessage.value("sender_id")=="1234"&&server.lastMessage.value("broadcaster_id")=="1234","Identity not bound to grant");pump(1200);check(server.sends==1&&!chat.scheduled(),"Multiple sources repeated POST or idle announcement timer running");
        chat.observe(1,snapshot("Cancel Me","Artist"));chat.observe(2,snapshot("Cancel Me","Artist"));pump(150);chat.configure(1,false,a.format,1000);chat.configure(2,false,a.format,1000);pump(1200);check(server.sends==1,"Disabling failed to cancel pending message");
        chat.configure(1,true,a.format,1000);await([&]{return chat.authorized();});chat.observe(1,snapshot("Stopped","Artist"));chat.broadcast(false,{});pump(1200);check(server.sends==1,"Stream stop failed to cancel pending message");
        chat.broadcast(true,"streamer");server.sent=false;await([&]{return server.sends==2;});pump(1200);check(server.sends==2,"Unconfirmed message retried");check(chat.forget()&&!chat.authorized(),"Local disconnect retained authorization");chat.remove(1);chat.remove(2);
    }
    {
        Server bad;bad.wrongUrl=true;obs::ChatAnnouncer chat(temp.filePath("bad.pcs"),bad.endpoints());chat.configure(1,true,a.format,1000);chat.authorize();await([&]{return bad.devices==1;});pump(100);check(chat.code().isEmpty()&&!chat.authorized()&&bad.polls==0,"Untrusted OAuth URL accepted");
    }
    {
        Server missing;missing.scope=false;const auto pth=temp.filePath("missing.pcs");grant(pth);obs::ChatAnnouncer chat(pth,missing.endpoints());chat.configure(1,true,a.format,1000);await([&]{return missing.validations==1;});pump(100);check(!chat.authorized(),"Missing chat permission accepted");
    }
    {
        Server renew;renew.rejectValidation=true;const auto pth=temp.filePath("renew.pcs");grant(pth);obs::ChatAnnouncer chat(pth,renew.endpoints());chat.configure(1,true,a.format,1000);await([&]{return chat.authorized();});check(renew.refreshes==1&&renew.validations==2,"Expired grant not refreshed and revalidated");const auto decoded=QJsonDocument::fromJson(*companion::ProtectedStore(pth).load()).object();check(decoded.value("refresh")=="rotated-refresh","Rotated refresh not persisted");
    }
    if(argc>1){
        const auto output=QString::fromUtf8(argv[1]);QDir().mkpath(output);
        const char *languages[]={"es","en","it","fr","de"};
        const char *captions[]={"Autorizar chat de Twitch","Authorize Twitch chat","Autorizza la chat di Twitch","Autoriser le chat Twitch","Twitch-Chat autorisieren"};
        for(int language=0;language<5;++language){ui::setLanguage(languages[language]);obs::ChatAnnouncer chat(temp.filePath(QString(languages[language])+".pcs"),server.endpoints());auto *d=chat.dialog(nullptr);pump(30);auto *button=d->findChild<QPushButton*>("chatAuthorize");check(button&&button->text()==QString::fromUtf8(captions[language]),"Untranslated chat authorization");check(button->rect().width()>=button->fontMetrics().horizontalAdvance(button->text())+12,"Translated chat button clipped");check(d->grab().save(output+"/"+languages[language]+".png"),"Chat dialog capture");d->close();pump(5);}
    }
    std::cout<<"PASS chat checks="<<checks<<"; local HTTP only; no real Twitch messages\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<"\n";return 1;}}
