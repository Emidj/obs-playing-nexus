// SPDX-License-Identifier: MIT
#pragma once
#include "track-visual.hpp"
#include "adapters/obs-visual-renderer.hpp"
#include "normal-prepared.hpp"
namespace playing::visual {
// One reusable GPU target per renderer host. Destruction requires graphics
// context, like the shared renderer. No readback, raster work or scene ownership.
class TransitionSurface final {
public:
    TransitionSurface()=default;
    TransitionSurface(const TransitionSurface &)=delete;
    TransitionSurface &operator=(const TransitionSurface &)=delete;
    ~TransitionSurface();
    void draw(oto::ObsVisualRenderer &,const TrackFramePtr &);
private:
    gs_texrender_t *target_=nullptr;
    gs_effect_t *effect_=nullptr;
};
// Stateless composition through the single renderer owned by the host.
// Caller holds OBS graphics context; no Core refresh, GUI preparation or I/O.
// Inactive/null returns before touching the renderer or requiring a context.
void drawTrack(oto::ObsVisualRenderer &renderer,const TrackFramePtr &frame,bool active=true,TransitionSurface *surface=nullptr);
// Immutable host sample only; no shaping, rasterization, clock or I/O here.
void drawNativeNormal(oto::ObsVisualRenderer &renderer,
    const normal::PreparedNormalPtr &frame,const normal::TimelineSample &sample,
    double speed=40,double pause=2,const animation::Sample *motion=nullptr);
}
