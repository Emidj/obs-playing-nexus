// SPDX-License-Identifier: MIT
#pragma once
#include "playing/providers/provider-registry.hpp"
#include "playing/visual/track-visual.hpp"
#include "playing/visual/artwork-loader.hpp"
#include "playing/companion/profile.hpp"
#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <map>
#include <functional>
namespace playing::obs {
// GUI-owned foundation for native sources. No OBS callbacks/graphics here.
// A publication may outlive its consumer and may be read on the render thread.
struct Consumer {
    visual::TrackPublication publication;
    std::atomic<providers::ProviderIssue> issue{providers::ProviderIssue::None};
    // GUI-only raw Track State notification, independent of visual hiding/transitions.
    std::function<void(core::SnapshotPtr)> trackChanged;
};
struct SchedulingStats {
    std::uint64_t animationPasses=0,animationVisits=0,staticSkips=0,dynamicCopies=0;
};
class SessionHost final : public QObject {
public:
    explicit SessionHost(const std::string &trustedDatabaseLibrary = {},QString assetRoot={});
    ~SessionHost() override;
    using Id = std::uint64_t;
    Id attach(providers::Request request, visual::TrackLayout layout,
        std::shared_ptr<Consumer> consumer, bool showExisting = true, int hideAfterMs = 0);
    // Explicit local fictitious preview; no DJ reader, network or polling demand.
    Id attachDemo(core::TrackState,visual::TrackLayout,std::shared_ptr<Consumer>);
    void demoTrack(Id,core::TrackState);
    void active(Id id, bool value);
    void detach(Id id);
    // GUI-only visual/policy changes retain the acquired reader and Core.
    void configure(Id id, const visual::TrackLayout &layout, bool showExisting, int hideAfterMs);
    void refresh();
    // Protected local receiver profile -> explicit trusted channel/factory.
    // Paths/credentials stay in this GUI host, never exported in Track State.
    std::string remoteProfile(const QString &absolutePath);
    bool remoteConnected(const std::string &id) const;
    bool scheduled() const;
    bool animationScheduled() const;
    int pollingIntervalMs() const;
    int animationIntervalMs() const;
    std::size_t sessions() const;
    std::size_t consumers() const;
    std::uint64_t refreshes() const;
    std::string failure() const;
    oto::visual::VisualStats visualStats() const;
    SchedulingStats schedulingStats() const;
private:
    void owner() const;
    void demand();
    void updateCadence();
    void publishFrames(bool animationOnly=false);
    void artwork(Id id);
    void publishScheduled(bool animationOnly=false) noexcept;
    struct Remote;
    std::map<std::string, std::unique_ptr<Remote>> remotes_;
    struct Entry {
        providers::Request request;
        visual::TrackLayout layout;
        std::shared_ptr<Consumer> consumer;
        providers::ConsumerToken token;
        core::SnapshotPtr last;
        bool showExisting = true;
        int hideAfterMs = 0;
        std::uint64_t initialTrack = 0, visibleTrack = 0;
        qint64 deadline = 0;
        std::shared_ptr<visual::normal::Timeline> timeline;
        visual::TrackFramePtr normalBase;
        visual::TrackFramePtr idleFrame;
        core::SnapshotPtr imageSnapshot;
        oto::visual::FramePtr image;
        std::uint64_t imageGeneration=0;
        visual::animation::Clock animationClock;
        std::shared_ptr<core::PlayingCore> demoCore;
        bool demoActive=false;
        bool needsAnimation=false;
        double animationRateShift=0,decorationRateShift=0;
        core::SnapshotPtr observed;
    };
    providers::ProviderRegistry registry_;
    visual::TrackVisualHost visual_;
    visual::ArtworkLoader images_;
    QTimer timer_;
    QTimer expiry_;
    QTimer animation_;
    QElapsedTimer clock_;
    std::map<Id, Entry> entries_;
    Id sequence_ = 0;
    std::uint64_t refreshes_ = 0;
    SchedulingStats schedulingStats_;
    std::string failure_;
};
}
