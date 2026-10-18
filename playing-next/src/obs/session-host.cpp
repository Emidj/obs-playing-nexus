// SPDX-License-Identifier: MIT
#include "playing/obs/session-host.hpp"
#include "playing/visual/performance-profile.hpp"
#include "playing/providers/virtualdj-live.hpp"
#include "playing/providers/serato-live.hpp"
#include "playing/providers/rekordbox-live.hpp"
#include <QGuiApplication>
#include <QFileInfo>
#include <QThread>
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include "playing/companion/protected-store.hpp"
#include <QCryptographicHash>
namespace playing::obs {
struct SessionHost::Remote {
    std::shared_ptr<providers::network::Channel> channel=std::make_shared<providers::network::Channel>();
    companion::Receiver receiver{channel};
    companion::ReceiverSettings settings;
    QByteArray payloadHash;
    bool demand=false,reconnect=false;
};
std::string SessionHost::remoteProfile(const QString &absolutePath) {
    owner();
    const QFileInfo file(absolutePath);
    if(!file.isAbsolute()||!file.isFile())throw std::invalid_argument("No linked DJ profile; use pairing first");
    const auto path=file.canonicalFilePath();
    const auto payload=companion::ProtectedStore(path).load();
    if(!payload)throw std::invalid_argument("No linked DJ profile");
    const auto profile=companion::decodeProfile(*payload);
    if(!profile.receiver)throw std::invalid_argument("No linked DJ; use pairing first");
    const auto id="network-"+QCryptographicHash::hash(path.toUtf8(),QCryptographicHash::Sha256).toHex().left(24).toStdString();
    const auto hash=QCryptographicHash::hash(*payload,QCryptographicHash::Sha256);
    auto found=remotes_.find(id);
    if(found==remotes_.end()) {
        auto remote=std::make_unique<Remote>();
        if(providers::network::registerProvider(registry_,remote->channel,id)!=providers::Registration::Registered)
            throw std::invalid_argument("Remote profile capacity reached");
        remote->receiver.changed=[this,id] {
            const auto result=registry_.refresh({id,providers::Mode::Automatic,{}});
            for(auto &entry:entries_)if(entry.second.token&&entry.second.request.providerId==id)
                entry.second.consumer->issue.store(result.issue);
            publishScheduled();
        };
        found=remotes_.emplace(id,std::move(remote)).first;
    }
    auto &remote=*found->second;
    if(remote.payloadHash!=hash) {
        remote.receiver.stop();remote.demand=false;
        remote.settings=*profile.receiver;remote.reconnect=profile.autoReconnect;remote.payloadHash=hash;
        demand();
    }
    return id;
}
bool SessionHost::remoteConnected(const std::string &id) const {
    owner();const auto found=remotes_.find(id);return found!=remotes_.end()&&found->second->receiver.connected();
}
void SessionHost::owner() const {
    if (!qobject_cast<QGuiApplication *>(QCoreApplication::instance()) ||
        QThread::currentThread() != qApp->thread())
        throw std::logic_error("SessionHost requires GUI owner");
}
SessionHost::SessionHost(const std::string &trustedDatabaseLibrary,QString assetRoot):images_(8*1024*1024,32,std::move(assetRoot)) {
    owner();
    clock_.start();
    expiry_.setSingleShot(true);
    connect(&expiry_, &QTimer::timeout, this, [this] { publishScheduled(); });
    animation_.setInterval(16);
    connect(&animation_, &QTimer::timeout, this, [this] { publishScheduled(true); });
    providers::registerVirtualDj(registry_);
    providers::registerSerato(registry_);
    if (!trustedDatabaseLibrary.empty()) {
        const QFileInfo file(QString::fromUtf8(trustedDatabaseLibrary.c_str()));
        if (!file.isAbsolute() || !file.isFile()) throw std::invalid_argument("Require trusted absolute DB library");
        providers::rekordbox::registerProvider(registry_, file.canonicalFilePath().toUtf8().toStdString());
    }
    timer_.setInterval(100);
    QObject::connect(&timer_, &QTimer::timeout, this, [this] {
        try { refresh(); }
        catch (const std::exception &error) {
            failure_ = error.what(); timer_.stop();
            for (auto &item : entries_) item.second.consumer->publication.publish({});
            registry_.clear();
            for (auto &item : entries_) { item.second.token = {}; item.second.last.reset(); }
            demand();
        }
    });
}
SessionHost::~SessionHost() {
    // Destruction, like all mutation, must be marshalled by the source host.
    if (QThread::currentThread() != thread()) std::terminate();
    timer_.stop();
    animation_.stop();
    expiry_.stop();
    for(auto &remote:remotes_){remote.second->receiver.changed={};remote.second->receiver.stop();}
    for (auto &item : entries_) {
        item.second.consumer->publication.publish({});
        item.second.consumer->issue.store(providers::ProviderIssue::None);
    }
    registry_.clear();
    images_.clear();
    visual_.clear();
}
SessionHost::Id SessionHost::attach(providers::Request request, visual::TrackLayout layout,
    std::shared_ptr<Consumer> consumer, bool showExisting, int hideAfterMs) {
    owner();
    if (hideAfterMs < 0 || hideAfterMs > 3600000) throw std::invalid_argument("Invalid visibility duration");
    if (!consumer || entries_.size() >= providers::ProviderRegistry::maxConsumers)
        throw std::invalid_argument("Invalid source consumer");
    for (const auto &item : entries_) if (item.second.consumer == consumer)
        throw std::invalid_argument("Consumer already attached");
    if (!request.historyPath.empty()) {
        const QFileInfo file(QString::fromUtf8(request.historyPath.c_str()));
        if (!file.isAbsolute()) throw std::invalid_argument("History must be absolute");
        const auto canonical = file.canonicalFilePath();
        request.historyPath = (canonical.isEmpty() ? file.absoluteFilePath() : canonical).toUtf8().toStdString();
    }
    // Validate geometry even before metadata arrives; inactive cannot hide errors.
    visual::validateTrackLayout(layout);
    consumer->publication.publish({});
    consumer->issue.store(providers::ProviderIssue::None);
    const auto id = ++sequence_;
    entries_.emplace(id, Entry{std::move(request), std::move(layout), std::move(consumer), {}, {}, showExisting, hideAfterMs});
    auto &entry=entries_.at(id);
    entry.timeline=std::make_shared<visual::normal::Timeline>();
    entry.timeline->configure(800,visual::normal::Transition::Fade,clock_.elapsed());
    return id;
}
void SessionHost::updateCadence(){
    int polling=250,animation=33;bool local=false,active=false;
    for(const auto &item:entries_){const auto &entry=item.second;if(!entry.token&&!entry.demoActive)continue;active=true;const auto cadence=visual::performanceCadence(entry.layout.performanceProfile);animation=std::min(animation,cadence.animationMs);if(entry.token&&remotes_.find(entry.request.providerId)==remotes_.end()){local=true;polling=std::min(polling,cadence.pollingMs);}}
    if(!local)polling=100;if(!active)animation=16;
    if(timer_.interval()!=polling)timer_.setInterval(polling);if(animation_.interval()!=animation)animation_.setInterval(animation);
}
void SessionHost::demand() {
    updateCadence();
    bool local=false;
    const auto requests=registry_.activeRequests();
    for(const auto &request:requests)if(remotes_.find(request.providerId)==remotes_.end())local=true;
    for(auto &item:remotes_) {
        auto &remote=*item.second;
        const bool needed=std::any_of(requests.begin(),requests.end(),[&item](const auto &request){return request.providerId==item.first;});
        if(needed==remote.demand)continue;
        remote.demand=needed;
        if(needed){remote.receiver.setAutoReconnect(remote.reconnect);remote.receiver.connectTo(remote.settings);}
        else remote.receiver.stop();
    }
    if (local) { if (!timer_.isActive()) timer_.start(); }
    else timer_.stop();
    if(!consumers()){images_.clear();visual_.clear();expiry_.stop();animation_.stop();}
}
SessionHost::Id SessionHost::attachDemo(core::TrackState track,visual::TrackLayout layout,std::shared_ptr<Consumer> consumer){
    owner();if(!core::validMetadata(track))throw std::invalid_argument("Invalid fictitious track");
    const auto id=attach({"demo-preview",providers::Mode::Automatic,{}},std::move(layout),std::move(consumer));
    auto &entry=entries_.at(id);entry.demoCore=std::make_shared<core::PlayingCore>();entry.demoCore->start();demoTrack(id,std::move(track));return id;
}
void SessionHost::demoTrack(Id id,core::TrackState track){
    owner();auto found=entries_.find(id);if(found==entries_.end()||!found->second.demoCore)throw std::invalid_argument("Not a fictitious preview");
    track.identity.reset();track.origin=core::TrackOrigin{"demo","Fictitious local preview"};
    if(!core::validMetadata(track))throw std::invalid_argument("Invalid fictitious track");
    found->second.demoCore->observe({core::Playback::Playing,std::move(track)});
    if(found->second.demoActive)publishFrames();
}
void SessionHost::active(Id id, bool value) {
    owner();
    auto found = entries_.find(id);
    if (found == entries_.end()) return;
    auto &entry = found->second;
    if (value == (entry.demoCore?entry.demoActive:static_cast<bool>(entry.token))) return;
    if (value) {
        if(entry.demoCore){entry.demoActive=true;demand();publishFrames();return;}
        auto acquired = registry_.acquire(entry.request);
        if (acquired.result != providers::Acquisition::Acquired)
            throw std::invalid_argument("DJ request could not be acquired");
        entry.token = acquired.token;
    } else {
        entry.animationClock.visible(false,clock_.elapsed());
        images_.cancel(id);entry.imageSnapshot.reset();entry.image.reset();++entry.imageGeneration;
        registry_.release(entry.token); entry.token = {};entry.demoActive=false;entry.needsAnimation=false;
        entry.last.reset(); entry.consumer->publication.publish({});
        entry.initialTrack=entry.visibleTrack=0;entry.deadline=0;
        entry.timeline->visible(false,clock_.elapsed());entry.normalBase.reset();entry.idleFrame.reset();
        entry.consumer->issue.store(providers::ProviderIssue::None);
    }
    demand();
}
void SessionHost::detach(Id id) {
    owner();
    auto found = entries_.find(id);
    if (found == entries_.end()) return;
    active(id, false);
    found->second.consumer->publication.publish({});
    entries_.erase(found); demand();
}
void SessionHost::refresh() {
    owner();
    if (!registry_.consumerCount()) return;
    ++refreshes_;
    for (const auto &request : registry_.activeRequests()) {
        const auto result = registry_.refresh(request);
        for (auto &item : entries_) {
            auto &entry = item.second;
            if (entry.token && !(entry.request < request) && !(request < entry.request))
                entry.consumer->issue.store(result.issue);
        }
    }
    publishFrames();
}
void SessionHost::configure(Id id,const visual::TrackLayout &layout,bool showExisting,int hideAfterMs) {
    owner();visual::validateTrackLayout(layout);
    if(hideAfterMs<0||hideAfterMs>3600000)throw std::invalid_argument("Invalid visibility duration");
    auto found=entries_.find(id);if(found==entries_.end())return;
    auto &entry=found->second;entry.idleFrame.reset();
    if(entry.layout.performanceProfile!=layout.performanceProfile){entry.layout.performanceProfile=layout.performanceProfile;updateCadence();}
    const auto sameNode=[](const auto &a,const auto &b){return a.box==b.box&&a.text==b.text&&a.family==b.family&&a.pixelSize==b.pixelSize&&a.minimumPixelSize==b.minimumPixelSize&&a.color==b.color&&a.overflow==b.overflow&&a.horizontal==b.horizontal&&a.vertical==b.vertical&&a.bold==b.bold&&a.italic==b.italic&&a.outline==b.outline&&a.gradientBottom==b.gradientBottom&&a.outlineWidth==b.outlineWidth;};
    const bool changed=entry.layout.animationProfile!=layout.animationProfile||entry.layout.artworkEnabled!=layout.artworkEnabled||entry.layout.profileFallback!=layout.profileFallback||entry.layout.artworkBox!=layout.artworkBox||!(entry.layout.streamerProfile==layout.streamerProfile)||entry.layout.normalPreset!=layout.normalPreset||entry.layout.normalTransition!=layout.normalTransition||entry.layout.normalDuration!=layout.normalDuration
        ||entry.layout.normalGap!=layout.normalGap||entry.layout.normalSpeed!=layout.normalSpeed||entry.layout.normalPause!=layout.normalPause||!(entry.layout.normalText==layout.normalText)
        ||entry.layout.canvas!=layout.canvas||!(entry.layout.textEffects==layout.textEffects)||!sameNode(entry.layout.title,layout.title)||!sameNode(entry.layout.artist,layout.artist);
    const bool policy=entry.layout.proPanelOpacity!=layout.proPanelOpacity||entry.layout.animationSpeed!=layout.animationSpeed||entry.layout.titlePingPong!=layout.titlePingPong||entry.layout.artistPingPong!=layout.artistPingPong||entry.layout.proPreset!=layout.proPreset||entry.layout.proHue!=layout.proHue||entry.layout.proLightIntensity!=layout.proLightIntensity||entry.layout.proLightPulse!=layout.proLightPulse||entry.layout.wholeTransition!=layout.wholeTransition||entry.showExisting!=showExisting||entry.hideAfterMs!=hideAfterMs;
    const bool appearanceOnly=!changed&&entry.layout.titlePingPong==layout.titlePingPong&&entry.layout.artistPingPong==layout.artistPingPong&&entry.layout.proPreset==layout.proPreset&&entry.layout.wholeTransition==layout.wholeTransition&&entry.showExisting==showExisting&&entry.hideAfterMs==hideAfterMs;
    auto positionedTitle=entry.layout.title,positionedArtist=entry.layout.artist;positionedTitle.box.moveTopLeft(layout.title.box.topLeft());positionedArtist.box.moveTopLeft(layout.artist.box.topLeft());
    if(changed&&!policy&&layout.proPreset&&entry.layout.canvas==layout.canvas&&entry.layout.artworkBox==layout.artworkBox&&entry.layout.artworkEnabled==layout.artworkEnabled&&entry.layout.streamerProfile==layout.streamerProfile&&entry.layout.profileFallback==layout.profileFallback&&entry.layout.normalPreset==layout.normalPreset&&entry.layout.animationProfile==layout.animationProfile&&entry.layout.normalTransition==layout.normalTransition&&entry.layout.normalDuration==layout.normalDuration&&entry.layout.normalGap==layout.normalGap&&entry.layout.normalSpeed==layout.normalSpeed&&entry.layout.normalPause==layout.normalPause&&sameNode(positionedTitle,layout.title)&&sameNode(positionedArtist,layout.artist)){
        if(const auto current=entry.consumer->publication.read();current&&current->snapshot&&current->snapshot->track&&current->snapshot->playback==core::Playback::Playing){const auto prepared=visual_.prepare(current->snapshot,layout);auto frame=std::make_shared<visual::TrackFrame>(*current);frame->title=prepared->title;frame->artist=prepared->artist;frame->titlePosition=prepared->titlePosition;frame->artistPosition=prepared->artistPosition;frame->scrollingText=prepared->scrollingText;frame->scrollingViewport=prepared->scrollingViewport;frame->scrollingDistance=prepared->scrollingDistance;entry.normalBase=frame;entry.consumer->publication.publish(frame);}
        entry.layout=layout;publishFrames();return;
    }
    if(policy&&appearanceOnly){
        // Replace immutable art atomically; keep timeline, artwork and reader alive.
        const double rateDifference=(entry.layout.animationSpeed-layout.animationSpeed)/100.0;
        entry.animationRateShift+=entry.animationClock.elapsed(clock_.elapsed())*rateDifference;
        entry.decorationRateShift+=entry.timeline->sample(clock_.elapsed()).decorationMilliseconds*rateDifference;
        entry.layout=layout;
        if(entry.normalBase&&layout.proPreset){
            const auto prepared=visual_.prepare(entry.normalBase->snapshot,layout);
            auto basis=std::make_shared<visual::TrackFrame>(*entry.normalBase);basis->proArt=prepared->proArt;basis->lightGain=prepared->lightGain;basis->proHue=prepared->proHue;basis->panelGain=prepared->panelGain;entry.normalBase=basis;
            if(const auto current=entry.consumer->publication.read()){auto frame=std::make_shared<visual::TrackFrame>(*current);frame->proArt=prepared->proArt;frame->lightGain=prepared->lightGain;frame->proHue=prepared->proHue;frame->panelGain=prepared->panelGain;entry.consumer->publication.publish(frame);}
        }
        publishFrames();return;
    }
    if(!changed&&!policy){
        if((entry.token||entry.demoActive)&&!entry.consumer->publication.read()){entry.last.reset();publishFrames();}
        return;
    }
    entry.layout=layout;entry.last.reset();
    entry.animationClock.reset(clock_.elapsed());
    entry.animationRateShift=entry.decorationRateShift=0;
    images_.cancel(id);entry.imageSnapshot.reset();entry.image.reset();++entry.imageGeneration;
    entry.normalBase.reset();entry.idleFrame.reset();
    entry.timeline->visible(false,clock_.elapsed()); // Reconfigure cancels OUT/IN without restarting reader.
    if(entry.showExisting!=showExisting)entry.initialTrack=0;
    if(entry.hideAfterMs!=hideAfterMs)entry.deadline=entry.visibleTrack&&hideAfterMs?clock_.elapsed()+hideAfterMs:0;
    entry.showExisting=showExisting;entry.hideAfterMs=hideAfterMs;
    if(entry.token||entry.demoActive)publishFrames();
}
void SessionHost::artwork(Id id) {
    auto &entry=entries_.at(id);auto frame=entry.consumer->publication.read();
    if(!frame||!entry.layout.artworkEnabled||!frame->snapshot||!frame->snapshot->track|| (frame->normalFrame&&frame->normalFrame->track!=frame->snapshot->track)) {
        images_.cancel(id);entry.imageSnapshot.reset();entry.image.reset();++entry.imageGeneration;return;
    }
    if(entry.imageSnapshot!=frame->snapshot) {
        entry.imageSnapshot=frame->snapshot;entry.image.reset();const auto generation=++entry.imageGeneration;
        images_.request(id,frame->snapshot,entry.layout.streamerProfile,entry.layout.profileFallback,true,[this,id,generation](const visual::ArtworkResult &result){
            try {
            const auto found=entries_.find(id);if(found==entries_.end())return;
            auto &target=found->second;if(generation!=target.imageGeneration||(!target.token&&!target.demoActive))return;
            auto current=target.consumer->publication.read();if(!current||current->snapshot!=target.imageSnapshot)return;
            auto box=target.layout.artworkBox.intersected(QRect(QPoint{},current->canvas));
            target.image=box.isEmpty()?oto::visual::FramePtr{}:visual_.prepareProArtwork(result.image,target.layout);
            auto copy=std::make_shared<visual::TrackFrame>(*current);copy->artwork=target.image;copy->artworkPosition=box.topLeft();target.consumer->publication.publish(copy);
            } catch(const std::exception &error) {failure_=error.what();}
        });
    }
    if(frame->artwork!=entry.image){auto copy=std::make_shared<visual::TrackFrame>(*frame);copy->artwork=entry.image;copy->artworkPosition=entry.layout.artworkBox.topLeft();entry.consumer->publication.publish(copy);}
}
void SessionHost::publishFrames(bool animationOnly) {
    const auto now=clock_.elapsed();qint64 earliest=0;bool animate=false;
    if(animationOnly)++schedulingStats_.animationPasses;
    for (auto &item : entries_) {
        auto &entry = item.second;
        if (!entry.token&&!entry.demoActive) continue;
        if(animationOnly&&!entry.needsAnimation){++schedulingStats_.staticSkips;continue;}
        if(animationOnly)++schedulingStats_.animationVisits;
        bool entryAnimating=false;
        const auto snapshot = entry.demoCore?entry.demoCore->snapshot():registry_.snapshot(entry.token);
        if(snapshot && snapshot->track && !entry.initialTrack)entry.initialTrack=snapshot->trackRevision;
        const bool hidden=snapshot && !entry.showExisting && snapshot->trackRevision==entry.initialTrack;
        if(snapshot && snapshot->track && snapshot->playback==core::Playback::Playing && !hidden && entry.visibleTrack!=snapshot->trackRevision){
            entry.visibleTrack=snapshot->trackRevision;entry.deadline=entry.hideAfterMs?now+entry.hideAfterMs:0;
        }
        const bool playable=snapshot&&snapshot->running&&snapshot->track&&snapshot->playback==core::Playback::Playing;
        const bool suppressed=hidden||(entry.deadline&&now>=entry.deadline)||!playable;
        if(suppressed){
            entry.timeline->receive({},now);entry.timeline->visible(false,now);entry.animationClock.visible(false,now);
            if(!entry.idleFrame){
                if(entry.normalBase&&entry.normalBase->normalFrame){auto idle=std::make_shared<visual::TrackFrame>(*entry.normalBase);auto normal=std::make_shared<visual::normal::PreparedNormal>(*idle->normalFrame);normal->text={};normal->track.reset();idle->normalFrame=normal;idle->title.reset();idle->artist.reset();idle->artwork.reset();idle->scrollingText={};idle->normalSample.track.reset();idle->normalSample.visible=true;idle->normalSample.transition={};idle->wholeTransition=false;entry.idleFrame=idle;}
                else entry.idleFrame=visual_.prepareIdle(entry.layout);
            }
            const auto current=entry.consumer->publication.read();
            if(!current||current->snapshot!=snapshot||current->title||current->artist||current->artwork||current->canvas!=entry.idleFrame->canvas||current->proArt!=entry.idleFrame->proArt||current->proHue!=entry.idleFrame->proHue||current->panelGain!=entry.idleFrame->panelGain||current->lightGain!=entry.idleFrame->lightGain){auto idle=std::make_shared<visual::TrackFrame>(*entry.idleFrame);idle->snapshot=snapshot;entry.consumer->publication.publish(idle);}
            images_.cancel(item.first);entry.imageSnapshot.reset();entry.image.reset();entry.needsAnimation=false;entry.last=snapshot;
            continue;
        }
        {
            entry.timeline->configure(entry.layout.normalDuration,entry.layout.normalTransition,now);
            entry.timeline->receive(playable?snapshot->track:visual::normal::TrackPtr{},now);
            entry.timeline->visible(!suppressed,now);
            const auto sample=entry.timeline->sample(now);
            if(suppressed||!sample.track){
                if(!entry.idleFrame)entry.idleFrame=visual_.prepareIdle(entry.layout);
                auto idle=std::make_shared<visual::TrackFrame>(*entry.idleFrame);idle->snapshot=snapshot;
                entry.consumer->publication.publish(idle);
            }
            else {
                if(!entry.normalBase||entry.normalBase->snapshot->track!=sample.track) {
                    if(entry.layout.normalPreset) {
                        auto basis=std::make_shared<visual::TrackFrame>();basis->snapshot=snapshot;
                        basis->normalFrame=visual_.prepareNormal(visual::normal::presetFromSetting(entry.layout.normalPreset),sample.track,entry.layout.canvas.width(),entry.layout.normalGap,entry.layout.normalText);
                        basis->normalSpeed=entry.layout.normalSpeed;basis->normalPause=entry.layout.normalPause;
                        basis->canvas=basis->normalFrame->layout.canvas;entry.normalBase=basis;
                    } else entry.normalBase=visual_.prepare(snapshot,entry.layout);
                }
                if(entry.normalBase->snapshot!=snapshot&&snapshot->track==sample.track) {
                    auto basis=std::make_shared<visual::TrackFrame>(*entry.normalBase);basis->snapshot=snapshot;entry.normalBase=basis;
                }
                const auto normal=entry.normalBase->normalFrame;
                const bool overflow=(normal&&(normal->layout.rows[0].overflow>0||normal->layout.rows[1].overflow>0||normal->layout.rows[2].overflow>0))||entry.normalBase->scrollingDistance[0]>0||entry.normalBase->scrollingDistance[1]>0;
                const bool moving=entry.timeline->needsFrames(normal&&normal->animatedDecoration,overflow);
                const auto current=entry.consumer->publication.read();
                if(moving||snapshot!=entry.last||!current||current->normalFrame!=entry.normalBase->normalFrame||current->normalSample.transitioning) {
                    auto frame=std::make_shared<visual::TrackFrame>(*entry.normalBase);frame->normalSample=sample;
                    frame->normalSample.decorationMilliseconds=std::max(0.0,frame->normalSample.decorationMilliseconds*entry.layout.animationSpeed/100.0+entry.decorationRateShift);
                    frame->wholeTransition=!entry.layout.proPreset&&(!normal||entry.layout.wholeTransition);
                    entry.consumer->publication.publish(frame);
                }
                entryAnimating=entryAnimating||moving;
            }
        }
        if(entry.deadline>now && (!earliest || entry.deadline<earliest))earliest=entry.deadline;
        artwork(item.first);
        entry.animationClock.visible(!suppressed,now);
        const auto elapsed=std::max(0.0,entry.animationClock.elapsed(now)*entry.layout.animationSpeed/100.0+entry.animationRateShift);
        const auto &program=visual::animation::program(entry.layout.animationProfile);
        if(const auto current=entry.consumer->publication.read()) {
            // Copy once for all dynamic fields, and publish only the final sample.
            // The render thread keeps its immutable previous frame throughout.
            std::shared_ptr<visual::TrackFrame> changed;
            const auto writable=[&]() -> visual::TrackFrame & {
                if(!changed){changed=std::make_shared<visual::TrackFrame>(*current);++schedulingStats_.dynamicCopies;}
                return *changed;
            };
            const bool moving=program.needsFrames(elapsed);
            if(entry.layout.animationProfile){const auto sample=program.sample(elapsed);if(!std::equal(sample.begin(),sample.end(),current->layerMotion.begin(),visual::animation::same))writable().layerMotion=sample;}
            entryAnimating=entryAnimating||moving;
            if(entry.layout.proPreset) {
                double period=0;
                switch(entry.layout.proPreset){
                case 3:period=10000;break;case 4:period=8000;break;
                case 5:period=2800;break;case 6:period=4200;break;
                case 7:period=4000;break;case 8:period=4200;break;
                case 9:period=4800;break;case 10:period=3600;break;
                case 11:period=6200;break;case 13:period=3200;break;
                case 14:period=5600;break;default:break;
                }
                if(period&&(entry.layout.proLightIntensity>0||entry.layout.proPreset==8)){
                    double sweep=std::fmod(elapsed,period)/period;
                    if(entry.layout.proPreset==3){const double span=current->canvas.width()*.14;sweep=sweep*(current->canvas.width()+span)-span;}
                    if(current->borderSweep!=sweep)writable().borderSweep=sweep;
                    entryAnimating=true;
                }
                if(entry.layout.proPreset==2||entry.layout.proPreset==12){
                    const double angle=std::fmod(elapsed*.06,360.0);
                    const double shimmer=.06+.025*std::sin(elapsed*.0062831853)+.01*std::sin(elapsed*.0157079633);
                    if(current->recordAngle!=angle||current->recordShimmer!=shimmer){auto &frame=writable();frame.recordAngle=angle;frame.recordShimmer=shimmer;}
                    entryAnimating=true;
                }
                const bool pulse=entry.layout.proLightPulse&&entry.layout.proLightIntensity>0;
                const double gain=entry.layout.proLightIntensity/100.0*(pulse?(.65+.35*visual::animation::oscillation(elapsed,4000)):1);
                if(current->lightGain!=gain)writable().lightGain=gain;
                entryAnimating=entryAnimating||pulse;
            }
            if(changed)entry.consumer->publication.publish(std::move(changed));
        }
        entry.needsAnimation=entryAnimating;animate=animate||entryAnimating;
        entry.last = snapshot;
    }
    if(!animationOnly){if(earliest)expiry_.start(static_cast<int>(std::max<qint64>(1,earliest-now)));else expiry_.stop();}
    if(animate){if(!animation_.isActive())animation_.start();}else animation_.stop();
}
void SessionHost::publishScheduled(bool animationOnly) noexcept {
    try {publishFrames(animationOnly);}
    catch(const std::exception &error) {
        failure_=error.what();animation_.stop();expiry_.stop();timer_.stop();
        for(auto &item:entries_) {
            item.second.consumer->publication.publish({});item.second.normalBase.reset();
            item.second.timeline->visible(false,clock_.elapsed());item.second.token={};item.second.last.reset();
        }
        registry_.clear();demand();
    }
}
bool SessionHost::scheduled() const { owner(); return timer_.isActive(); }
bool SessionHost::animationScheduled() const { owner(); return animation_.isActive(); }
int SessionHost::pollingIntervalMs()const{owner();return timer_.interval();}
int SessionHost::animationIntervalMs()const{owner();return animation_.interval();}
std::size_t SessionHost::sessions() const { owner(); return registry_.activeRequests().size(); }
std::size_t SessionHost::consumers() const { owner(); return registry_.consumerCount()+std::count_if(entries_.begin(),entries_.end(),[](const auto &item){return item.second.demoActive;}); }
std::uint64_t SessionHost::refreshes() const { owner(); return refreshes_; }
oto::visual::VisualStats SessionHost::visualStats() const { owner(); return visual_.stats(); }
SchedulingStats SessionHost::schedulingStats() const {owner();return schedulingStats_;}
std::string SessionHost::failure() const { owner(); return failure_; }
}

