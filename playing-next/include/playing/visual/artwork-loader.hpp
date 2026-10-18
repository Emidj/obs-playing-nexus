// SPDX-License-Identifier: MIT
#pragma once
#include "playing/core/playing-core.hpp"
#include "visual/scene.hpp"
#include <QObject>
#include <functional>
#include <memory>

namespace playing::visual {
enum class ImageOutcome { Ready, Missing, Failed, Unsupported, Capacity };
struct ArtworkResult {
    ImageOutcome outcome=ImageOutcome::Missing;
    oto::visual::ImagePtr image;
    std::uint64_t trackRevision=0;
    bool usedProfile=false;
};
struct ArtworkLoadStats {
    std::uint64_t reads=0,hits=0,evictions=0;
    std::size_t entries=0,bytes=0,pending=0,consumers=0;
};
// One per visual host. All API calls/destruction on its GUI owner thread.
// Acquisition/decode on one demand-driven worker; delivery queued to the owner.
// No Core mutation, timers, network, OBS calls or file watching. A new request
// for a consumer cancels its previous delivery, including profile-only changes.
class ArtworkLoader final : public QObject {
public:
    using Consumer=std::uint64_t;
    using Callback=std::function<void(const ArtworkResult &)>;
    explicit ArtworkLoader(std::size_t maxBytes=8*1024*1024,std::size_t maxEntries=32,QString assetRoot={});
    ~ArtworkLoader() override;
    void request(Consumer consumer,core::SnapshotPtr snapshot,
        std::optional<core::ImageReference> profile,bool fallback,bool active,Callback callback);
    void cancel(Consumer consumer);
    void clear(); // Cancels deliveries and clears cache; old jobs cannot repopulate it.
    ArtworkLoadStats stats() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
