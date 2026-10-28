// SPDX-License-Identifier: MIT
#include "playing/companion/detector.hpp"
#include "playing/companion/profile.hpp"
#include "playing/companion/protected-store.hpp"
#include "playing/providers/virtualdj-history.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QElapsedTimer>
#include <QProcess>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QSslSocket>
#include <windows.h>
#include <iostream>
#include <stdexcept>
using namespace playing::companion;
using namespace playing::providers;
namespace net=playing::providers::network;
namespace {
unsigned checks=0;
void check(bool ok,const char *message) {++checks; if(!ok) throw std::runtime_error(message);}
QByteArray read(const QString &path) {QFile f(path); if(!f.open(QIODevice::ReadOnly)) return {}; return f.read(65537);}
void write(const QString &path,const QByteArray &bytes) {
    // The other fixture process briefly opens these files with QFile. Windows
    // can deny atomic replacement during that read; retain atomic writes and
    // retry only that sharing/permission race, with a finite 250ms deadline.
    QElapsedTimer clock;clock.start();QString error;
    for(;;){
        QSaveFile f(path);f.setDirectWriteFallback(false);
        if(!f.open(QIODevice::WriteOnly)||f.write(bytes)!=bytes.size())
            throw std::runtime_error(("Fixture write "+path+": "+f.errorString()).toStdString());
        if(f.commit())return;
        error=f.errorString();
        if(clock.elapsed()>=250||(f.error()!=QFileDevice::RenameError&&f.error()!=QFileDevice::PermissionsError))
            throw std::runtime_error(("Fixture atomic replace "+path+": "+error).toStdString());
        QThread::msleep(5);
    }
}
bool wait(const std::function<bool()> &condition,int ms=7000) {QElapsedTimer t; t.start(); while(!condition() && t.elapsed()<ms) {QCoreApplication::processEvents(); QThread::msleep(2);} return condition();}
int child(const QString &mode,const QString &root) {
    const auto p=decodeProfile(*ProtectedStore(root+"/profile.pcs").load()); QTimer control; control.setInterval(25);
    QElapsedTimer diagnosticClock; diagnosticClock.start(); QByteArray trace;
    ProviderRegistry registry; Sender sender; Detector detector(registry,sender);
    auto channel=std::make_shared<net::Channel>(); Receiver receiver(channel); ConsumerToken token; std::uint64_t sequence=0;
    if(mode=="sender") {
        check(registerVirtualDjHistory(registry)==Registration::Registered,"Child history registration");
        check(detector.select(Request{"virtualdj",Mode::History,(root+"/history.txt").toUtf8().toStdString()}),"Child history selection");
        sender.clientsChanged=[&] {detector.syncDemand();};
        detector.changed=[&] {write(root+"/sender-state.txt",QByteArray::number(diagnosticClock.elapsed())+"ms detection="+QByteArray::number(static_cast<int>(detector.status()))+" issue="+QByteArray::number(static_cast<int>(detector.issue()))+" playback="+QByteArray::number(static_cast<int>(detector.snapshot()->playback)));};
        check(sender.pair(p.devices[0].credential),"Child pairing");
        check(sender.listen(p.identity,"127.0.0.1",0),"Child listener");
        const auto serverPort=sender.port();
        write(root+"/port.txt",QByteArray::number(sender.port()));
        QObject::connect(&control,&QTimer::timeout,[&,serverPort] {const auto command=read(root+"/sender.command");
            if(command=="pause") {sender.stop(); write(root+"/paused.txt","ok");}
            if(command=="resume" && sender.port()==0) {check(sender.listen(p.identity,"127.0.0.1",serverPort),"Child listener restart"); write(root+"/resumed.txt","ok");}
            if(command=="revoke") {sender.revoke(p.devices[0].credential.id); write(root+"/revoked.txt","ok");} if(command=="stop") QCoreApplication::quit();});
    } else {
        check(mode=="receiver","Child mode"); check(net::registerProvider(registry,channel)==Registration::Registered,"Child network registration");
        const Request request{"network",Mode::Automatic,""}; token=registry.acquire(request).token;
        receiver.changed=[&,request] {
            if(!receiver.lastError().isEmpty()) std::cerr<<"Receiver transport: "<<receiver.lastError().toStdString()<<'\n';
            registry.refresh(request); const auto frame=net::encode(net::fromSnapshot(*registry.snapshot(token),"bridge",++sequence));
            if(trace.size()<8000) {trace+=QByteArray::number(diagnosticClock.elapsed())+"ms connected="+QByteArray::number(receiver.connected())+" failure="+QByteArray::number(static_cast<int>(receiver.lastFailure()))+" playback="+QByteArray::number(static_cast<int>(registry.snapshot(token)->playback))+"\n"; write(root+"/receiver-trace.txt",trace);}
            if(!frame) throw std::runtime_error("Bridge frame invalid");
            write(root+"/bridge.pnx",QByteArray(frame->data(),static_cast<qsizetype>(frame->size())));
        };
        receiver.setAutoReconnect(p.autoReconnect); auto settings=*p.receiver; settings.port=static_cast<quint16>(read(root+"/port.txt").toUInt()); check(receiver.connectTo(settings),"Child connect");
        QObject::connect(&control,&QTimer::timeout,[&] {if(read(root+"/receiver.command")=="stop") QCoreApplication::quit();});
    }
    control.start(); QTimer deadline; deadline.setSingleShot(true); QObject::connect(&deadline,&QTimer::timeout,[] {QCoreApplication::exit(2);}); deadline.start(55000);
    const auto result=QCoreApplication::exec(); receiver.changed={}; receiver.stop(); sender.stop(); detector.setPreview(false); sender.clientsChanged={}; if(token) registry.release(token); return result;
}
bool state(const QString &root,playing::core::Playback playback,const std::string &title={}) {
    const auto bytes=read(root+"/bridge.pnx"); if(bytes.isEmpty()) return false;
    const auto frame=net::decode(std::string_view(bytes.constData(),static_cast<std::size_t>(bytes.size()))); return frame && frame->observation.playback==playback && (title.empty() || (frame->observation.metadata && frame->observation.metadata->title==title));
}
int run(const QString &root,bool obs) {
    QDir().mkpath(root); Profile p; p.identity=createIdentity(); p.devices.push_back({"STREAM-PC",newDeviceCredential()});
    p.receiver=ReceiverSettings{"127.0.0.1",9000,"OBS Playing Companion",certificatePin(p.identity.certificate),p.devices[0].credential};
    p.autoReconnect=!obs;
    ProtectedStore(root+"/profile.pcs").save(encodeProfile(p)); write(root+"/history.txt",QByteArray::fromStdString(u8"21:03 : Björk - Jóga 🎵\r\n"));
    QProcess sender,receiver;
    for(auto process:{&sender,&receiver}) process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *arguments) {arguments->flags|=CREATE_NO_WINDOW;});
    const auto close=[&] {write(root+"/sender.command","stop"); write(root+"/receiver.command","stop");};
    try {
        sender.start(QCoreApplication::applicationFilePath(),{"--child","sender",root}); check(sender.waitForStarted(),"Separate sender startup");
        check(wait([&] {return !read(root+"/port.txt").isEmpty();}),"Separate sender listener");
        receiver.start(QCoreApplication::applicationFilePath(),{"--child","receiver",root}); check(receiver.waitForStarted(),"Separate receiver startup");
        check(sender.processId()!=receiver.processId() && sender.processId()!=QCoreApplication::applicationPid(),"Independent process identities");
        check(wait([&] {return state(root,playing::core::Playback::Playing,u8"Jóga 🎵");}),"Separate TLS/native history");
        write(root+"/ready.txt","ready");
        if(obs) check(wait([&] {return read(root+"/obs.command")=="1";},35000),"OBS initial snapshot acknowledgment");
        write(root+"/history.txt",QByteArray::fromStdString(u8"21:04 : Björk - Next 🎵\r\n"));
        check(wait([&] {return state(root,playing::core::Playback::Playing,u8"Next 🎵");}),"Separate track change");
        if(!obs) {
            write(root+"/sender.command","pause"); check(wait([&] {return !read(root+"/paused.txt").isEmpty() && state(root,playing::core::Playback::Unavailable);}),"Separate outage did not clear state");
            write(root+"/sender.command","resume"); check(wait([&] {return !read(root+"/resumed.txt").isEmpty() && state(root,playing::core::Playback::Playing,u8"Next 🎵");}),"Separate automatic reconnection");
        }
        if(obs) check(wait([&] {return read(root+"/obs.command")=="2";},12000),"OBS changed snapshot acknowledgment");
        write(root+"/sender.command","revoke"); check(wait([&] {return !read(root+"/revoked.txt").isEmpty() && state(root,playing::core::Playback::Unavailable);}),"Separate revocation clears track");
        if(obs) check(wait([&] {return read(root+"/obs.command")=="3";},12000),"OBS disconnect acknowledgment");
        close(); check((sender.state()==QProcess::NotRunning || sender.waitForFinished(5000)) && sender.exitStatus()==QProcess::NormalExit && sender.exitCode()==0,"Sender normal exit");
        check((receiver.state()==QProcess::NotRunning || receiver.waitForFinished(5000)) && receiver.exitStatus()==QProcess::NormalExit && receiver.exitCode()==0,"Receiver normal exit");
        write(root+"/process-result.txt","PASS independent processes; TLS/native History/track change/revoke; exits=0,0\n"); return 0;
    } catch(...) {close(); sender.waitForFinished(2000); receiver.waitForFinished(2000); const auto a=sender.readAllStandardError(),b=receiver.readAllStandardError(); write(root+"/sender-error.txt",a); write(root+"/receiver-error.txt",b); std::cerr<<"Sender diagnostics: "<<a.constData()<<"Receiver diagnostics: "<<b.constData(); throw;}
}
}
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    try {check(QSslSocket::setActiveBackend("schannel"),"Schannel backend");
        const auto args=app.arguments(); if(args.size()==4 && args[1]=="--child") return child(args[2],args[3]);
        if(args.size()==3 && args[1]=="--obs") return run(args[2],true);
        QTemporaryDir dir(QDir::currentPath()+"/companion-process-XXXXXX"); check(dir.isValid(),"Process fixture directory");
        try {const auto result=run(dir.path(),false); std::cout<<"PASS "<<checks<<" independent Companion process checks; loopback only\n"; return result;}
        catch(...) {dir.setAutoRemove(false); std::cerr<<"Failure fixture preserved: "<<dir.path().toStdString()<<'\n'; throw;}
    } catch(const std::exception &e) {std::cerr<<"FAIL "<<e.what()<<'\n'; return 1;}
}
