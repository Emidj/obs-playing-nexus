// SPDX-License-Identifier: MIT
#include "playing/providers/network-provider.hpp"
#include <atomic>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <limits>
using namespace playing::core;
using namespace playing::providers;
namespace net = playing::providers::network;
namespace {
unsigned checks=0;
void check(bool value) { ++checks; if(!value) throw std::runtime_error("Network check " + std::to_string(checks)); }
TrackState track() {
    TrackState t{u8"Jóga 🎵",u8"Björk"}; t.identity=TrackIdentity{"library-1","song-123"};
    t.origin=TrackOrigin{"serato","automatic"};
    t.artwork={ArtworkStatus::Ready,ImageReference{ImageKind::Https,"https://example.test/cover.png","v1"}}; return t;
}
net::Message song(std::uint64_t seq=1,const std::string &stream="stream-A") {
    return {stream,seq,net::MessageKind::Snapshot,{Playback::Playing,track()}};
}
std::string bytes(const net::Message &message) { const auto b=net::encode(message); if(!b) throw std::runtime_error("Fixture encode failed"); return *b; }
void codec() {
    const auto m=song(); const auto b=bytes(m); const auto d=net::decode(b);
    check(d && d->stream==m.stream && d->sequence==m.sequence && d->observation.metadata==m.observation.metadata);
    check(bytes(*d)==b);
    bool rejected=true; for(std::size_t i=0;i<b.size();++i) rejected=rejected&&!net::decode(std::string_view(b).substr(0,i)); check(rejected);
    check(!net::decode(b+"extra")); check(!net::decode(std::string(net::maxFrameBytes+1,'x')));
    auto bad=b; bad[7]='2'; check(!net::decode(bad)); bad=b; bad[0]=1; check(!net::decode(bad));
    bad=b; bad[8]=static_cast<char>(99); check(!net::decode(bad));
    const std::size_t activeFlag=8+1+2+m.stream.size()+8+1;
    bad=b; bad[activeFlag]=2; check(!net::decode(bad));
    bad=b; const auto title=bad.find(m.observation.metadata->title); bad[title]=static_cast<char>(255); check(!net::decode(bad));
    bad=b; const auto url=bad.find("https://"); bad[url-3]=static_cast<char>(ImageKind::LocalFile);
    bad.replace(url,8,"C:/aaaaa"); check(!net::decode(bad)); // Crafted remote local path, valid locally but not portable.
    auto invalid=m; invalid.sequence=0; check(!net::encode(invalid)); invalid=m; invalid.stream="bad stream"; check(!net::encode(invalid));
    invalid=m; invalid.stream=std::string(129,'a'); check(!net::encode(invalid));
    invalid=m; invalid.observation.metadata->title=std::string(4097,'x'); check(!net::encode(invalid));
    invalid=m; invalid.observation.metadata->title=std::string("a\0b",3); check(!net::encode(invalid));
    invalid=m; invalid.kind=static_cast<net::MessageKind>(99); check(!net::encode(invalid));
    invalid=m; invalid.observation.playback=static_cast<Playback>(99); check(!net::encode(invalid));
    invalid=m; invalid.observation.retainedTrack=track(); check(!net::encode(invalid));
    for(auto status:{ArtworkStatus::Unknown,ArtworkStatus::Missing,ArtworkStatus::Pending,ArtworkStatus::Failed}) {
        auto x=m; x.observation.metadata->artwork={status,std::nullopt}; const auto result=net::decode(bytes(x));
        check(result && result->observation.metadata==x.observation.metadata);
    }
    for(auto kind:{ImageKind::LocalFile,ImageKind::Asset}) {
        auto x=m; x.observation.metadata->artwork={ArtworkStatus::Ready,ImageReference{kind,kind==ImageKind::LocalFile?"C:/DJ/cover.png":"dj-asset","v1"}};
        const auto result=net::decode(bytes(x)); check(result && result->observation.metadata->artwork.status==ArtworkStatus::Missing);
        check(resolveArtwork(result->observation.metadata->artwork,ImageReference{ImageKind::Asset,"streamer",""})->location=="streamer");
    }
    for(auto state:{Playback::Waiting,Playback::Paused,Playback::Unavailable}) {
        net::Message x{"stream-A",1,net::MessageKind::Snapshot,{state,std::nullopt}}; check(net::decode(bytes(x)).has_value());
    }
    net::Message heartbeat{"stream-A",2,net::MessageKind::Heartbeat,{}}; check(net::decode(bytes(heartbeat)).has_value());
    heartbeat.observation.metadata=track(); check(!net::encode(heartbeat));
    PlayingCore core; core.start(); core.observe({Playback::Playing,track()}); core.observe({Playback::Paused,std::nullopt});
    const auto paused=net::decode(bytes(net::fromSnapshot(*core.snapshot(),"stream-A",3)));
    check(paused && paused->observation.replaceRetainedTrack && paused->observation.retainedTrack==std::optional<TrackState>{track()});
    PlayingCore receiver; receiver.start(); check(receiver.observe(paused->observation)==Update::Applied);
    check(!receiver.snapshot()->metadata && receiver.snapshot()->track && *receiver.snapshot()->track==track());
    check(receiver.snapshot()->trackRevision!=core.snapshot()->trackRevision);
    check(receiver.observe(paused->observation)==Update::Unchanged);
    auto empty=*paused; empty.observation.retainedTrack.reset(); check(receiver.observe(net::decode(bytes(empty))->observation)==Update::Applied);
    check(!receiver.snapshot()->track);
    core.stop(); check(net::fromSnapshot(*core.snapshot(),"stream-A",4).observation.playback==Playback::Unavailable);
}
void channel() {
    net::Channel channel(5); std::uint64_t cursor=0;
    check(channel.sample(cursor).kind==SampleKind::Unavailable); check(channel.sample(cursor).kind==SampleKind::NoChange);
    check(channel.receive(bytes(song()),0)==net::Ingress::Disconnected);
    check(!channel.connect("bad stream",0)); check(!channel.connect("stream-A",-1));
    check(channel.connect("stream-A",0)); check(!channel.connect("stream-A",0));
    check(channel.sample(cursor).observation->playback==Playback::Waiting);
    check(channel.receive(bytes(song()),1)==net::Ingress::Accepted);
    const auto sample=channel.sample(cursor); check(sample.observation->metadata==song().observation.metadata);
    check(channel.receive(bytes(song()),2)==net::Ingress::Replayed);
    check(channel.receive(bytes(song(2,"wrong")),2)==net::Ingress::WrongStream);
    check(channel.receive("invalid",2)==net::Ingress::Invalid);
    check(channel.receive(bytes(song(2)),0)==net::Ingress::InvalidClock);
    check(channel.receive(bytes(song(2)),std::numeric_limits<double>::quiet_NaN())==net::Ingress::InvalidClock);
    net::Message heartbeat{"stream-A",2,net::MessageKind::Heartbeat,{}};
    check(channel.receive(bytes(heartbeat),4)==net::Ingress::Accepted); check(channel.sample(cursor).kind==SampleKind::NoChange);
    check(!channel.expire(8)); check(channel.expire(9)); check(channel.sample(cursor).kind==SampleKind::Unavailable);
    check(channel.receive(bytes(song(3)),9)==net::Ingress::Disconnected); check(!channel.connect("stream-A",9));
    check(channel.connect("stream-B",9)); check(channel.receive(bytes(song(4,"stream-A")),10)==net::Ingress::WrongStream);
    check(channel.receive(bytes(song(1,"stream-B")),14)==net::Ingress::Disconnected); // Deadline cannot be resurrected by late traffic.
    check(channel.sample(cursor).kind==SampleKind::Unavailable);
    check(channel.connect("stream-C",15)); check(channel.receive(bytes(song(1,"stream-C")),16)==net::Ingress::Accepted);
    check(channel.receive(bytes(song(3,"stream-C")),17)==net::Ingress::Accepted); check(channel.receive(bytes(song(2,"stream-C")),18)==net::Ingress::Replayed);
    check(channel.sample(cursor).observation->metadata.has_value()); channel.disconnect(); check(channel.sample(cursor).kind==SampleKind::Unavailable);
    bool rejected=false; try { net::Channel invalid(0); } catch(const std::invalid_argument &) { rejected=true; } check(rejected);
}
void artwork() {
    net::Channel channel;std::uint64_t cursor=0;
    const Artwork cover{ArtworkStatus::Ready,ImageReference{ImageKind::LocalFile,"C:/local-cache/cover.png","hash"}};
    check(channel.connect("images-A",0));check(!channel.acceptsArtwork("images-A",1));
    check(channel.receive(bytes(song(1,"images-A")),1)==net::Ingress::Accepted);
    const auto original=channel.sample(cursor);
    check(channel.acceptsArtwork("images-A",1));check(!channel.acceptsArtwork("images-B",1));
    check(channel.receive(bytes({"images-A",2,net::MessageKind::Heartbeat,{}}),2)==net::Ingress::Accepted);
    check(channel.acceptsArtwork("images-A",1));check(channel.updateArtwork("images-A",1,cover));
    check(channel.sample(cursor).observation->metadata->artwork==cover);
    check(original.observation->metadata->artwork.image->kind==ImageKind::Https);
    check(channel.updateArtwork("images-A",1,cover));check(channel.sample(cursor).kind==SampleKind::NoChange);
    check(!channel.updateArtwork("images-A",1,{ArtworkStatus::Ready,{}}));
    check(channel.receive(bytes(song(3,"images-A")),3)==net::Ingress::Accepted);
    check(!channel.updateArtwork("images-A",1,cover));check(channel.acceptsArtwork("images-A",3));
    auto paused=song(4,"images-A");paused.observation={Playback::Paused,std::nullopt,track(),true};
    check(channel.receive(bytes(paused),4)==net::Ingress::Accepted);check(channel.updateArtwork("images-A",4,cover));
    check(channel.sample(cursor).observation->retainedTrack->artwork==cover);
    channel.disconnect();check(!channel.updateArtwork("images-A",4,cover));
    check(channel.connect("images-B",5));check(!channel.acceptsArtwork("images-A",4));
    check(channel.receive(bytes(song(1,"images-B")),6)==net::Ingress::Accepted);
    check(channel.expire(21));check(!channel.updateArtwork("images-B",1,cover));
}
void shared() {
    ProviderRegistry registry; auto channel=std::make_shared<net::Channel>();
    check(net::registerProvider(registry,channel)==Registration::Registered);
    check(net::registerProvider(registry,channel)==Registration::Duplicate);
    check(net::registerProvider(registry,nullptr,"null")==Registration::Invalid);
    const Request request{"network",Mode::Automatic,""};
    const auto a=registry.acquire(request),b=registry.acquire(request); check(registry.consumerCount()==2);
    check(registry.refresh(request).result==Refresh::Unavailable);
    channel->connect("stream-A",0); channel->receive(bytes(song()),1);
    check(registry.refresh(request).result==Refresh::Applied); const auto first=registry.snapshot(a.token);
    check(first==registry.snapshot(b.token)); check(first->track->origin->provider=="serato");
    check(registry.refresh(request).result==Refresh::Unchanged && registry.snapshot(a.token)==first);
    channel->disconnect(); check(registry.refresh(request).result==Refresh::Unavailable && !registry.snapshot(a.token)->track);
    registry.release(a.token); check(registry.snapshot(b.token)!=nullptr); registry.release(b.token);
    check(registry.acquire({"network",Mode::History,""}).result==Acquisition::UnsupportedMode);
    // Event ingress may run on a transport thread; only owner refreshes Core.
    const auto c=registry.acquire(request); check(channel->connect("stream-B",2));
    std::atomic<bool> valid{true}; std::thread sender([&] { for(unsigned i=1;i<=1000;++i)
        if(channel->receive(bytes(song(i,"stream-B")),3)!=net::Ingress::Accepted) valid=false; });
    sender.join(); check(valid); check(registry.refresh(request).result==Refresh::Applied);
    check(registry.snapshot(c.token)->track->title==track().title);
    registry.release(c.token);
}
}
int main() { try { codec(); channel(); artwork(); shared(); std::cout<<checks<<" Network checks; versioned codec, paused snapshots, remote images, replay/expiry and event ingress (no LAN authentication)\n"; return 0; }
    catch(const std::exception &e) {std::cerr<<e.what()<<'\n'; return 1;} }
