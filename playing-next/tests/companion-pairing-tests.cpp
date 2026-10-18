// SPDX-License-Identifier: MIT
#include "playing/companion/pairing.hpp"
#include "playing/companion/profile.hpp"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QSslSocket>
#include <QThread>
#include <iostream>
#include <stdexcept>
using namespace playing::companion;
namespace {
unsigned checks=0;
void check(bool ok,const char *message) {++checks; if(!ok) throw std::runtime_error(message);}
bool wait(const std::function<bool()> &condition) {QElapsedTimer time; time.start(); while(!condition()&&time.elapsed()<6000) {QCoreApplication::processEvents(); QThread::msleep(1);} return condition();}
void test() {
    const auto identity=createIdentity(); Sender sender; PairingServer server; PairingClient client;
    check(sender.listen(identity,"127.0.0.1",0),"Data listener");
    check(!server.active()&&!client.searching(),"Unsolicited discovery");
    check(server.open(identity,sender.port(),"DJ-PC",QHostAddress::LocalHost,0),"Pair listener");
    check(client.search(QHostAddress::LocalHost,server.discoverySocketPort()),"Discovery query");
    check(wait([&] {return client.devices().size()==1;}),"Discovery response");
    const auto candidate=client.devices().front();
    check(candidate.name=="DJ-PC"&&candidate.address=="127.0.0.1"&&candidate.dataPort==sender.port()&&candidate.pin==certificatePin(identity.certificate),"Public advertisement");
    check(pairingFingerprint(candidate.pin).size()==39&&pairingFingerprint({}).isEmpty(),"Fingerprint bounds");
    QByteArray request; QString name; unsigned requests=0; bool completed=false; ReceiverSettings received;
    server.requested=[&](QByteArray id,QString deviceName) {request=id; name=deviceName; ++requests;};
    client.completed=[&](ReceiverSettings settings) {received=settings; completed=true;};
    check(client.request(candidate,"STREAM-PC"),"Pair request");
    check(wait([&] {return !request.isEmpty();}),"Pinned TLS request");
    check(server.pending(request)&&name=="STREAM-PC"&&!completed&&sender.clientCount()==0,"Credentials issued without approval");
    const auto credential=newDeviceCredential(); check(sender.pair(credential),"Authorize data credential");
    check(server.approve(request,credential),"Explicit approval");
    check(wait([&] {return completed;}),"Automatic credentials transfer");
    check(received.device.id==credential.id&&received.device.secret==credential.secret&&received.certificateSha256==candidate.pin&&received.port==sender.port(),"Transferred credential mismatch");
    check(!server.pending(request)&&!client.searching(),"Completed request retained");
    auto channel=std::make_shared<playing::providers::network::Channel>(); Receiver receiver(channel);
    check(receiver.connectTo(received),"Connect automatically paired device");
    check(wait([&] {return receiver.connected()&&sender.clientCount()==1;}),"Authenticated data connection");
    check(sender.revoke(credential.id),"Revoke guided pair");
    check(wait([&] {return !receiver.connected()&&sender.clientCount()==0;}),"Revoked client retained access");
    receiver.stop(); client.stop(); request.clear(); completed=false;
    check(client.request(candidate,"SECOND-PC"),"Second request");
    check(wait([&] {return !request.isEmpty();}),"Second pending request");
    server.reject(request);
    check(wait([&] {return !client.error().isEmpty();}),"Rejected client waits forever");
    check(!completed&&!server.pending(request),"Rejected request authorized");
    client.stop(); auto wrong=candidate; wrong.pin[0]=static_cast<char>(wrong.pin[0]^1); const auto before=requests;
    check(client.request(wrong,"WRONG-PIN"),"Wrong pin request start");
    check(wait([&] {return !client.error().isEmpty();}),"Wrong pin accepted");
    check(requests==before&&!completed,"Wrong identity reached approval");
    client.stop(); request.clear(); auto changed=candidate;
    changed.dataPort=static_cast<quint16>(candidate.dataPort==65535?65534:candidate.dataPort+1);
    check(client.request(changed,"CHANGED-ENDPOINT"),"Changed endpoint request");
    check(wait([&] {return !request.isEmpty();}),"Changed endpoint pending");
    check(server.approve(request,newDeviceCredential()),"Changed endpoint response");
    check(wait([&] {return !client.error().isEmpty();}),"Changed endpoint accepted");
    check(!completed,"Changed endpoint issued receiver settings");
    client.stop(); request.clear(); check(client.request(candidate,"CANCELLED-PC"),"Cancelled request");
    check(wait([&] {return !request.isEmpty();}),"Cancel pending");
    server.stop(); check(!server.pending(request)&&!server.active(),"Closed window retained request");
    check(!server.approve(request,newDeviceCredential()),"Expired approval accepted");
    check(wait([&] {return !client.error().isEmpty();}),"Window closure did not cancel request");
    check(server.open(identity,sender.port(),"DJ-PC",QHostAddress::LocalHost,0,100),"Short fixture window");
    check(wait([&] {return !server.active();}),"Pair window did not expire");
    check(!client.request(candidate,QString(81,'x')),"Unbounded device name");
    check(!client.search(QHostAddress("8.8.8.8")),"Public discovery accepted");
    client.stop(); sender.stop();
}
}
int main(int argc,char **argv) {QCoreApplication app(argc,argv); try {check(QSslSocket::setActiveBackend("schannel"),"TLS backend"); test(); std::cout<<"PASS "<<checks<<" guided pairing checks; loopback only\n"; return 0;} catch(const std::exception &e) {std::cerr<<"FAIL "<<e.what()<<'\n'; return 1;}}
