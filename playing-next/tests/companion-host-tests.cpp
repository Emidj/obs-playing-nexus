// SPDX-License-Identifier: MIT
#include "playing/companion/detector.hpp"
#include "playing/companion/protected-store.hpp"
#include "playing/providers/virtualdj-history.hpp"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QLockFile>
#include <QTemporaryDir>
#include <QThread>
#include <QSslSocket>
#include <iostream>
#include <stdexcept>
using namespace playing::core;
using namespace playing::providers;
using namespace playing::companion;
namespace {
unsigned checks=0;
void check(bool value,const char *message) {++checks; if(!value) throw std::runtime_error(message);}
template<class F> bool fails(F f) {try {f(); return false;} catch(const std::exception &) {return true;}}
void pump(int milliseconds=150) {QElapsedTimer t; t.start(); while(t.elapsed()<milliseconds) {QCoreApplication::processEvents(); QThread::msleep(1);}}
struct State {unsigned starts=0,stops=0,samples=0; Playback playback=Playback::Waiting;};
class Fixture final : public IDjProvider {
    std::shared_ptr<State> state_; bool started_=false;
public:
    explicit Fixture(std::shared_ptr<State> s):state_(std::move(s)) {}
    void start() override {started_=true; ++state_->starts;}
    void stop() noexcept override {if(started_) {started_=false; ++state_->stops;}}
    Sample sample() override {++state_->samples; return {SampleKind::Observation,Observation{state_->playback,state_->playback==Playback::Playing?std::optional<Metadata>{Metadata{u8"Canción 🎵","Artist"}}:std::nullopt}};}
};
void detector() {
    ProviderRegistry registry; Sender sender;
    auto a=std::make_shared<State>(),b=std::make_shared<State>();
    check(registry.registerProvider({"a","A",true,false},[a](const Request &) {return std::make_unique<Fixture>(a);})==Registration::Registered,"Register A");
    check(registry.registerProvider({"b","B",true,false},[b](const Request &) {return std::make_unique<Fixture>(b);})==Registration::Registered,"Register B");
    Detector d(registry,sender);
    check(a->starts==0 && b->starts==0,"Idle opened readers");
    d.setPreview(true); check(a->starts==1 && b->starts==1,"Demand missing");
    check(d.status()==Detection::Searching,"Initial search");
    a->playback=Playback::Playing; pump();
    check(d.status()==Detection::Selected && d.snapshot()->track->title==u8"Canción 🎵","Single candidate selection");
    b->playback=Playback::Playing; pump();
    check(d.status()==Detection::Ambiguous && !d.snapshot()->track && d.candidates().size()==2,"Ambiguity chose arbitrary provider");
    check(d.select(Request{"a",Mode::Automatic,""}),"Manual selection");
    check(d.status()==Detection::Selected && d.candidates().size()==1,"Manual resolution");
    const auto before=d.snapshot(); check(!d.select(Request{"missing",Mode::Automatic,""}) && d.snapshot()==before,"Invalid selection changed state");
    const auto shared=registry.acquire({"a",Mode::Automatic,""});
    const auto starts=a->starts; check(shared.result==Acquisition::Acquired && registry.snapshot(shared.token)==d.snapshot(),"Session sharing");
    a->playback=Playback::Paused; pump();
    check(d.snapshot()->playback==Playback::Paused && d.snapshot()->track && !d.snapshot()->metadata,"Paused track lost");
    d.setPreview(false); check(d.status()==Detection::Inactive && !d.snapshot()->track,"Inactive retained track");
    check(a->starts==starts && registry.activeRequests().size()==1,"Released another consumer");
    const auto samples=a->samples; pump(); check(a->samples==samples,"Inactive timer sampled");
    check(registry.release(shared.token) && registry.activeRequests().empty(),"Last consumer not released");
    check(a->stops==a->starts && b->stops==b->starts,"Reader lifecycle imbalance");
    bool rejected=false; std::thread wrong([&] {rejected=fails([&] {d.status();});}); wrong.join(); check(rejected,"Wrong thread accepted");
}
QByteArray read(const QString &path) {QFile f(path); if(!f.open(QIODevice::ReadOnly)) throw std::runtime_error("Read fixture"); return f.readAll();}
void write(const QString &path,const QByteArray &bytes) {QFile f(path); if(!f.open(QIODevice::WriteOnly) || f.write(bytes)!=bytes.size()) throw std::runtime_error("Write fixture");}
void store() {
    QTemporaryDir dir; check(dir.isValid(),"Temporary profile directory");
    const auto path=dir.filePath("credentials.bin"); ProtectedStore s(path);
    check(!s.load(),"Missing profile");
    const QByteArray secret="credential-test-secret-256-bits"; s.save(secret);
    const auto encrypted=read(path); check(!encrypted.contains(secret) && encrypted.startsWith("PCS1"),"Plaintext persisted");
    check(s.load()==secret,"DPAPI roundtrip");
    s.save("replacement"); check(s.load()==QByteArray("replacement"),"Atomic replacement");
    const auto valid=read(path);
    check(fails([&] {s.save(QByteArray(65537,'x'));}) && read(path)==valid,"Oversize damaged existing profile");
    {QLockFile lock(path+".lock"); check(lock.tryLock(0),"Fixture lock"); check(fails([&] {s.load();}) && fails([&] {s.save(secret);}),"Concurrent writer accepted");}
    auto corrupt=valid; corrupt[corrupt.size()-1]=static_cast<char>(corrupt.back()^1); write(path,corrupt);
    check(fails([&] {s.load();}),"Tampered ciphertext accepted");
    check(fails([&] {s.save(secret);}) && read(path)==corrupt,"Corrupt profile overwritten");
    write(path,valid+"extra"); check(fails([&] {s.load();}),"Trailing bytes accepted");
    write(path,"PCS"); check(fails([&] {s.load();}),"Truncated envelope accepted");
    write(path,"unrelated user document"); check(fails([&] {s.save(secret);}) && read(path)=="unrelated user document","Unknown data overwritten");
    check(fails([] {ProtectedStore relative("relative.bin");}),"Relative store accepted");
}
void nativePath(const QString &identityDir,const std::string &dbLibrary) {
    ProviderRegistry native;
    check(registerNativeDetectors(native,dbLibrary) && native.descriptors().size()==3,"Native factories registration");
    check(native.activeRequests().empty() && !registerNativeDetectors(native,dbLibrary),"Registration opened workers or accepted nonempty registry");
    QTemporaryDir dir; check(dir.isValid(),"History fixture directory");
    const auto history=dir.filePath("history.txt"); write(history,QByteArray::fromStdString(u8"21:03 : Björk - Jóga 🎵\r\n"));
    ProviderRegistry dj,remote; Sender sender;
    check(registerVirtualDjHistory(dj)==Registration::Registered,"History factory");
    Detector detector(dj,sender); sender.clientsChanged=[&] {detector.syncDemand();};
    check(!detector.select(Request{"virtualdj",Mode::History,"relative.txt"}),"Relative history path accepted");
    check(detector.select(Request{"virtualdj",Mode::History,history.toStdString()}),"History selection");
    const auto credential=newDeviceCredential(); check(sender.pair(credential),"Native path pairing");
    ServerIdentity identity{QSslCertificate(read(identityDir+"/certificate.pem")),QSslKey(read(identityDir+"/private-key.pem"),QSsl::Rsa)};
    check(sender.listen(identity,"127.0.0.1",0),"Native path TLS listener");
    auto channel=std::make_shared<playing::providers::network::Channel>();
    check(playing::providers::network::registerProvider(remote,channel)==Registration::Registered,"Remote factory");
    const Request request{"network",Mode::Automatic,""}; const auto consumer=remote.acquire(request);
    Receiver receiver(channel); receiver.changed=[&] {remote.refresh(request);};
    check(receiver.connectTo({"127.0.0.1",sender.port(),"OBS Playing Companion",certificatePin(identity.certificate),credential}),"Native path connect");
    QElapsedTimer timeout; timeout.start();
    while(!remote.snapshot(consumer.token)->track && timeout.elapsed()<5000) pump(10);
    const auto snapshot=remote.snapshot(consumer.token);
    check(snapshot->track && snapshot->track->title==u8"Jóga 🎵" && snapshot->track->artist==u8"Björk","Native history through detector/TLS/remote Core");
    check(detector.status()==Detection::Selected && dj.activeRequests().size()==1,"Authenticated demand not active");
    receiver.stop(); timeout.restart(); while(sender.clientCount()!=0 && timeout.elapsed()<5000) pump(10);
    check(sender.clientCount()==0 && detector.status()==Detection::Inactive && dj.activeRequests().empty(),"Last authenticated client kept worker");
    check(!remote.snapshot(consumer.token)->track,"Disconnect kept remote track");
    sender.clientsChanged={}; sender.stop(); remote.release(consumer.token);
}
}
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    try {check(argc==3,"Identity and DB dependency required"); check(QSslSocket::setActiveBackend("schannel"),"Schannel backend"); detector(); store(); nativePath(QString::fromLocal8Bit(argv[1]),argv[2]); std::cout<<"PASS "<<checks<<" Companion host checks\n"; return 0;}
    catch(const std::exception &e) {std::cerr<<"FAIL "<<e.what()<<'\n'; return 1;}
}
