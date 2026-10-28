// SPDX-License-Identifier: MIT
#include "playing/companion/profile.hpp"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QSslSocket>
#include <QThread>
#include <QTcpServer>
#include <iostream>
#include <stdexcept>
using namespace playing::companion;
using namespace playing::providers;
using namespace playing::core;
namespace net=playing::providers::network;
namespace {
unsigned checks=0;
void check(bool ok,const char *message) {++checks; if(!ok) throw std::runtime_error(message);}
bool wait(const std::function<bool()> &condition,int milliseconds=6000) {QElapsedTimer t; t.start(); while(!condition() && t.elapsed()<milliseconds) {QCoreApplication::processEvents(); QThread::msleep(1);} return condition();}
void pump(int milliseconds) {QElapsedTimer t; t.start(); wait([&] {return t.elapsed()>=milliseconds;},milliseconds+1000);}
void test() {
    const auto identity=createIdentity(); const auto credential=newDeviceCredential();
    Sender sender; check(sender.pair(credential) && sender.listen(identity,"127.0.0.1",0),"Sender fixture");
    const auto port=sender.port(); ReceiverSettings settings{"127.0.0.1",port,"OBS Playing Companion",certificatePin(identity.certificate),credential};
    auto channel=std::make_shared<net::Channel>(); ProviderRegistry registry;
    check(net::registerProvider(registry,channel)==Registration::Registered,"Remote factory");
    const Request request{"network",Mode::Automatic,""}; const auto token=registry.acquire(request).token;
    Receiver receiver(channel); receiver.changed=[&] {registry.refresh(request);};
    PlayingCore dj; dj.start(); dj.observe({Playback::Playing,Metadata{u8"Jóga 🎵",u8"Björk"}}); sender.publish(dj.snapshot());
    receiver.setAutoReconnect(true); check(receiver.connectTo(settings),"Initial connect");
    check(wait([&] {return receiver.connected() && registry.snapshot(token)->metadata.has_value();}),"Initial state");
    const auto oldToken=registry.snapshot(token)->trackRevision;
    sender.stop(); check(wait([&] {return receiver.reconnectPending();}),"Lost connection no retry");
    check(receiver.lastFailure()==ConnectionFailure::Transport && !registry.snapshot(token)->track,"Old song survived disconnection");
    dj.observe({Playback::Playing,Metadata{u8"Next 🎵",u8"Björk"}}); sender.publish(dj.snapshot());
    check(sender.listen(identity,"127.0.0.1",port),"Restart sender same port");
    check(wait([&] {return receiver.connected() && registry.snapshot(token)->metadata && registry.snapshot(token)->metadata->title==u8"Next 🎵";}),"Automatic reconnect restored state");
    check(registry.snapshot(token)->trackRevision!=oldToken && receiver.lastFailure()==ConnectionFailure::None,"Reconnected reused old identity token/error");
    check(sender.revoke(credential.id),"Revoke current credential");
    check(wait([&] {return receiver.lastFailure()==ConnectionFailure::Pairing && !receiver.reconnectPending();}),"Revoked credential kept retrying");
    check(!registry.snapshot(token)->track,"Revocation retained track");
    check(sender.pair(credential),"Restore credential for explicit retry"); receiver.stop();
    auto wrong=settings; wrong.certificateSha256=QByteArray(32,'x'); check(receiver.connectTo(wrong),"Wrong pin connect");
    check(wait([&] {return receiver.lastFailure()==ConnectionFailure::CertificatePin;}),"Pin diagnostic");
    check(!receiver.reconnectPending(),"Untrusted identity retried");
    wrong=settings; wrong.certificateName="wrong-name"; check(receiver.connectTo(wrong),"Wrong name connect");
    check(wait([&] {return receiver.lastFailure()==ConnectionFailure::CertificateValidation;}),"Certificate name diagnostic");
    check(!receiver.reconnectPending(),"Wrong certificate name retried");
    check(receiver.connectTo(settings) && wait([&] {return receiver.connected();}),"Explicit recovery after trust failure");
    sender.stop(); check(wait([&] {return receiver.reconnectPending();}),"Second retry scheduled");
    receiver.stop(); check(!receiver.reconnectPending() && receiver.lastFailure()==ConnectionFailure::None,"Manual stop did not cancel/reset");
    check(sender.listen(identity,"127.0.0.1",port),"Restart after manual stop"); pump(1200);
    check(!receiver.connected() && sender.clientCount()==0,"Manual stop silently reconnected");
    receiver.setAutoReconnect(false); check(receiver.connectTo(settings) && wait([&] {return receiver.connected();}),"Manual-only connect");
    sender.stop(); check(wait([&] {return receiver.lastFailure()!=ConnectionFailure::None;}),"Manual-only disconnection"); check(!receiver.reconnectPending(),"Retry without opt-in");
    receiver.setAutoReconnect(true); check(receiver.connectTo(settings),"Offline connect");
    check(wait([&] {return receiver.reconnectPending();}),"Connection refused not retryable");
    receiver.setAutoReconnect(false); check(!receiver.reconnectPending(),"Disable retry did not cancel timer");
    wrong=settings; wrong.port=0; check(!receiver.connectTo(wrong) && receiver.lastFailure()==ConnectionFailure::InvalidSettings && !receiver.reconnectPending(),"Invalid settings retry/diagnostic");
    receiver.stop(); registry.release(token);
}
}
int main(int argc,char **argv) {QCoreApplication app(argc,argv); try {check(QSslSocket::setActiveBackend("schannel"),"Schannel"); test(); std::cout<<"PASS "<<checks<<" reconnect/typed diagnostic checks; loopback\n"; return 0;} catch(const std::exception &e) {std::cerr<<"FAIL "<<e.what()<<'\n'; return 1;}}
