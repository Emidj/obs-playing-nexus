// SPDX-License-Identifier: MIT
#include "playing/providers/network-provider.hpp"
#include <cmath>
#include <stdexcept>
#include <utility>

namespace playing::providers::network {
namespace {
bool streamValid(const std::string &s) {
    if (s.empty() || s.size() > 128) return false;
    for (unsigned char c : s) if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
        || (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
    return true;
}
bool valid(const Message &m) {
    if (!streamValid(m.stream) || m.sequence == 0) return false;
    const auto &o = m.observation;
    if(o.replaceRetainedTrack && o.playback!=core::Playback::Paused) return false;
    if (m.kind == MessageKind::Heartbeat) return o.playback == core::Playback::Waiting && !o.metadata && !o.retainedTrack;
    if (m.kind != MessageKind::Snapshot) return false;
    switch (o.playback) {
    case core::Playback::Playing: return o.metadata && core::validMetadata(*o.metadata) && !o.retainedTrack;
    case core::Playback::Paused: return !o.metadata && (!o.retainedTrack || core::validMetadata(*o.retainedTrack));
    case core::Playback::Waiting: case core::Playback::Unavailable: return !o.metadata && !o.retainedTrack;
    }
    return false;
}
struct Writer {
    std::string bytes;
    void integer(std::uint64_t value, unsigned count) { for (unsigned i=count; i>0; --i) bytes += static_cast<char>((value >> ((i-1)*8)) & 255); }
    void text(const std::string &value) { integer(value.size(), 2); bytes += value; }
    void track(const core::TrackState &t) {
        text(t.title); text(t.artist); integer(t.identity.has_value(),1);
        if(t.identity) { text(t.identity->scope); text(t.identity->value); }
        integer(t.origin.has_value(),1); if(t.origin) { text(t.origin->provider); text(t.origin->source); }
        integer(static_cast<unsigned>(t.artwork.status),1);
        if(t.artwork.image) { integer(static_cast<unsigned>(t.artwork.image->kind),1); text(t.artwork.image->location); text(t.artwork.image->version); }
    }
};
struct Reader {
    std::string_view bytes; std::size_t offset=0; bool ok=true;
    std::uint64_t integer(unsigned count) {
        if(count > bytes.size()-offset) {ok=false; return 0;}
        std::uint64_t value=0; for(unsigned i=0;i<count;++i) value=(value<<8)|static_cast<unsigned char>(bytes[offset++]); return value;
    }
    bool flag() { const auto v=integer(1); if(v>1) ok=false; return v==1; }
    std::string text() {
        const auto count=integer(2); if(!ok || count>4096 || count>bytes.size()-offset) {ok=false; return {};}
        std::string out(bytes.substr(offset,static_cast<std::size_t>(count))); offset+=static_cast<std::size_t>(count); return out;
    }
    core::TrackState track() {
        core::TrackState t; t.title=text(); t.artist=text();
        if(flag()) t.identity=core::TrackIdentity{text(),text()};
        if(flag()) t.origin=core::TrackOrigin{text(),text()};
        t.artwork.status=static_cast<core::ArtworkStatus>(integer(1));
        if(t.artwork.status==core::ArtworkStatus::Ready) {
            core::ImageReference image; image.kind=static_cast<core::ImageKind>(integer(1)); image.location=text(); image.version=text();
            t.artwork.image=std::move(image);
        }
        return t;
    }
};
void portable(core::TrackState &t) {
    if(t.artwork.image && t.artwork.image->kind != core::ImageKind::Https)
        t.artwork={core::ArtworkStatus::Missing,std::nullopt};
}
class Provider final : public IDjProvider {
public:
    explicit Provider(std::shared_ptr<Channel> channel) : channel_(std::move(channel)) {}
    void start() override { started_=true; cursor_=0; }
    void stop() noexcept override { started_=false; cursor_=0; }
    Sample sample() override { if(!started_) return {SampleKind::Unavailable}; return channel_->sample(cursor_); }
private:
    std::shared_ptr<Channel> channel_; bool started_=false; std::uint64_t cursor_=0;
};
}
std::optional<std::string> encode(const Message &message) {
    if(!valid(message)) return std::nullopt;
    auto m=message; if(m.observation.metadata) portable(*m.observation.metadata);
    if(m.observation.retainedTrack) portable(*m.observation.retainedTrack);
    Writer w; w.bytes="PNX1"; w.integer(static_cast<unsigned>(m.kind),1); w.text(m.stream); w.integer(m.sequence,8);
    w.integer(static_cast<unsigned>(m.observation.playback),1); w.integer(m.observation.metadata.has_value(),1);
    if(m.observation.metadata) w.track(*m.observation.metadata);
    w.integer(m.observation.retainedTrack.has_value(),1); if(m.observation.retainedTrack) w.track(*m.observation.retainedTrack);
    if(w.bytes.size()+4>maxFrameBytes) return std::nullopt;
    Writer framed; framed.integer(w.bytes.size(),4); framed.bytes+=w.bytes; return framed.bytes;
}
std::optional<Message> decode(std::string_view bytes) {
    if(bytes.size()<8 || bytes.size()>maxFrameBytes) return std::nullopt;
    Reader r{bytes}; if(r.integer(4)!=bytes.size()-4 || bytes.substr(4,4)!="PNX1") return std::nullopt;
    r.offset=8; Message m; m.kind=static_cast<MessageKind>(r.integer(1)); m.stream=r.text(); m.sequence=r.integer(8);
    m.observation.playback=static_cast<core::Playback>(r.integer(1));
    if(r.flag()) m.observation.metadata=r.track(); if(r.flag()) m.observation.retainedTrack=r.track();
    m.observation.replaceRetainedTrack=m.observation.playback==core::Playback::Paused;
    if(!r.ok || r.offset!=bytes.size() || !valid(m)) return std::nullopt;
    // Also reject non-portable references crafted without encode().
    const auto t=m.observation.metadata ? &*m.observation.metadata : m.observation.retainedTrack ? &*m.observation.retainedTrack : nullptr;
    if(t && t->artwork.image && t->artwork.image->kind != core::ImageKind::Https) return std::nullopt;
    return m;
}
Message fromSnapshot(const core::Snapshot &s, std::string stream, std::uint64_t sequence) {
    Message m; m.stream=std::move(stream); m.sequence=sequence;
    m.observation.playback=s.running ? s.playback : core::Playback::Unavailable;
    if(s.running && s.playback==core::Playback::Playing) m.observation.metadata=s.metadata;
    if(s.running && s.playback==core::Playback::Paused && s.track) m.observation.retainedTrack=*s.track;
    m.observation.replaceRetainedTrack=m.observation.playback==core::Playback::Paused;
    return m;
}
Channel::Channel(double timeout) : timeout_(timeout) {
    if(!std::isfinite(timeout) || timeout<1 || timeout>300) throw std::invalid_argument("Network timeout outside 1..300 seconds");
}
bool Channel::connect(std::string stream,double now) {
    if(!streamValid(stream) || !std::isfinite(now) || now<0) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    // A fresh epoch is mandatory. Host generates it after authenticated pairing.
    if(stream==lastStream_) return false;
    stream_=std::move(stream); lastStream_=stream_; sequence_=snapshotSequence_=0; lastActivity_=now;
    latest_={SampleKind::Observation,core::Observation{core::Playback::Waiting,std::nullopt}}; ++revision_; return true;
}
Ingress Channel::receive(std::string_view bytes,double now) {
    if(!std::isfinite(now) || now<0) return Ingress::InvalidClock;
    const auto m=decode(bytes); if(!m) return Ingress::Invalid;
    std::lock_guard<std::mutex> lock(mutex_);
    if(stream_.empty()) return Ingress::Disconnected;
    if(m->stream!=stream_) return Ingress::WrongStream;
    if(now<lastActivity_) return Ingress::InvalidClock;
    if(m->sequence<=sequence_) return Ingress::Replayed;
    if(now-lastActivity_>=timeout_) { stream_.clear(); latest_={SampleKind::Unavailable,std::nullopt,ProviderIssue::ConnectionLost}; ++revision_; return Ingress::Disconnected; }
    sequence_=m->sequence; lastActivity_=now;
    if(m->kind==MessageKind::Snapshot) { snapshotSequence_=m->sequence;latest_={SampleKind::Observation,m->observation}; ++revision_; }
    return Ingress::Accepted;
}
bool Channel::acceptsArtwork(const std::string &stream,std::uint64_t sequence) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !stream_.empty()&&stream_==stream&&sequence!=0&&sequence==snapshotSequence_
        &&latest_.observation&&(latest_.observation->metadata||latest_.observation->retainedTrack);
}
bool Channel::updateArtwork(const std::string &stream,std::uint64_t sequence,const core::Artwork &artwork) {
    if(!core::validArtwork(artwork))return false;
    std::lock_guard<std::mutex> lock(mutex_);
    if(stream_.empty()||stream_!=stream||sequence==0||sequence!=snapshotSequence_||!latest_.observation)return false;
    auto &observation=*latest_.observation;
    auto track=observation.metadata?&*observation.metadata:observation.retainedTrack?&*observation.retainedTrack:nullptr;
    if(!track)return false;
    if(!(track->artwork==artwork)){track->artwork=artwork;++revision_;}
    return true;
}
bool Channel::expire(double now) {
    if(!std::isfinite(now) || now<0) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    if(stream_.empty() || now<lastActivity_ || now-lastActivity_<timeout_) return false;
    stream_.clear(); latest_={SampleKind::Unavailable,std::nullopt,ProviderIssue::ConnectionLost}; ++revision_; return true;
}
void Channel::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    stream_.clear(); latest_={SampleKind::Unavailable,std::nullopt,ProviderIssue::ConnectionLost}; ++revision_;
}
Sample Channel::sample(std::uint64_t &cursor) const {
    std::lock_guard<std::mutex> lock(mutex_); if(cursor==revision_) return {};
    cursor=revision_; return latest_;
}
Registration registerProvider(ProviderRegistry &registry,std::shared_ptr<Channel> channel,std::string id) {
    if(!channel) return Registration::Invalid;
    return registry.registerProvider({std::move(id),"Remote Track State",true,false,true},[channel](const Request &request)->std::unique_ptr<IDjProvider> {
        if(request.mode!=Mode::Automatic) throw std::invalid_argument("Network Automatic only");
        return std::make_unique<Provider>(channel);
    });
}
} // namespace playing::providers::network
