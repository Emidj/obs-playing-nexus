// SPDX-License-Identifier: MIT
#pragma once
#include "playing/core/playing-core.hpp"
#include "visual/visual-engine.hpp"
#include "normal-prepared.hpp"
#include "animation-profiles.hpp"
#include <atomic>
#include <list>
#include <QPolygonF>
namespace playing::visual {
struct ProCoverGeometry {QRect box;QPolygonF quad;QImage mask;};
QImage horizontalReferenceImage(int style);
ProCoverGeometry horizontalCoverGeometry(int style,QSize canvas);
ProCoverGeometry proCoverGeometry(int style,QSize canvas);
// Host-supplied geometry/style, not a preset or source settings schema.
struct TrackLayout {
    int performanceProfile=1; // Consumer policy; excluded from portable appearance.
    normal::TextOverrides normalText;
    std::array<TextEffects,2> textEffects;
    int normalPreset=0; // 0 BASIC, 1 Prism, 2 Air, 3 Cantilever Next.
    bool wholeTransition=false; // NORMAL compatibility default; BASIC always whole.
    int proPreset=0,proHue=0,proLightIntensity=65,proPanelOpacity=100;
    bool proLightPulse=false;
    bool titlePingPong=false,artistPingPong=false;
    normal::Transition normalTransition=normal::Transition::Fade;
    int normalDuration=800,normalGap=12;
    double normalSpeed=40,normalPause=2;
    QSize canvas{640,120};
    oto::visual::TextNode title,artist;
    bool artworkEnabled=false,profileFallback=true;
    int animationProfile=0;
    int animationSpeed=100;
    QRect artworkBox{0,0,100,100};
    std::optional<core::ImageReference> streamerProfile;
};
// Checks geometry/style without shaping, rasterizing or accessing caches.
void validateTrackLayout(const TrackLayout &layout);
struct ProPrism {oto::visual::FramePtr pixels,light;QPoint position;};
struct ProArt {oto::visual::FramePtr backdrop,light,panel,record,recordInk,recordReflection,spray;QPoint recordPosition;std::vector<ProPrism> prisms;};
using ProArtPtr=std::shared_ptr<const ProArt>;
struct TrackFrame {
    normal::PreparedNormalPtr normalFrame;
    normal::TimelineSample normalSample;
    double normalSpeed=40,normalPause=2;
    core::SnapshotPtr snapshot;
    QSize canvas;
    oto::visual::FramePtr title,artist;
    QPoint titlePosition,artistPosition;
    oto::visual::FramePtr artwork;
    QPoint artworkPosition;
    animation::Sample layerMotion;
    bool wholeTransition=false;
    ProArtPtr proArt;
    double lightGain=0;
    double panelGain=1;
    double recordAngle=0,recordShimmer=0;
    int proHue=0,proStyle=0;double borderSweep=0;
    std::array<std::vector<normal::TextTile>,2> scrollingText;
    std::array<QRect,2> scrollingViewport;
    std::array<int,2> scrollingDistance{};
};
using TrackFramePtr=std::shared_ptr<const TrackFrame>;
// One instance per Playing host, shared by all source consumers. GUI owner only.
// Inactive/waiting/paused => empty. No reader, timer, image fetching or OBS calls.
class TrackVisualHost final {
public:
    oto::visual::FramePtr prepareArtwork(const oto::visual::ImagePtr &image,QSize size,bool fill=false,bool circular=false);
    oto::visual::FramePtr prepareProArtwork(const oto::visual::ImagePtr &image,const TrackLayout &layout);
    TrackFramePtr prepare(core::SnapshotPtr snapshot,const TrackLayout &layout,bool active=true);
    TrackFramePtr prepareIdle(const TrackLayout &layout);
    normal::PreparedNormalPtr prepareNormal(normal::NativePreset preset,normal::TrackPtr track,int width,int gap=12,const normal::TextOverrides& overrides={});
    oto::visual::VisualStats stats() const;
    void clear(); // GUI only; externally retained frames remain valid.
private:
    ProArtPtr prepareNeonClub(const TrackLayout &);
    struct ProCacheEntry {QSize canvas;QRect cover;int hue;std::weak_ptr<const ProArt> art;};
    std::list<ProCacheEntry> proCache_;
    oto::visual::VisualEngine engine_;
    struct NormalCacheEntry {normal::NativePreset preset;int width,gap;std::weak_ptr<const normal::PreparedNormal> frame;normal::TextOverrides overrides;};
    std::list<NormalCacheEntry> normalCache_; // Weak references; consumers own pixels.
};
// Atomic immutable handoff only. Host prepares on GUI, render reads const frames.
class TrackPublication final {
public:
    void publish(TrackFramePtr frame) {std::atomic_store(&frame_,std::move(frame));}
    TrackFramePtr read() const {return std::atomic_load(&frame_);}
private:
    TrackFramePtr frame_;
};
} // namespace playing::visual
