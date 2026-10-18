// SPDX-License-Identifier: MIT
#pragma once
#include "normal-motion.hpp"
#include "playing/core/playing-core.hpp"
#include <optional>
#include <stdexcept>
#include <thread>
namespace playing::visual::normal {
using TrackPtr=std::shared_ptr<const core::TrackState>;
struct TimelineSample {
    TrackPtr track;
    TransitionSample transition;
    double scrollSeconds=0;
    double decorationMilliseconds=0;
    bool visible=false,transitioning=false;
};
// Host-owned state machine. Clock values are supplied by the host; this object
// never schedules work. Render receives an immutable sample, never this object.
class Timeline final {
public:
    Timeline():owner_(std::this_thread::get_id()){}
    Timeline(const Timeline&)=delete;
    Timeline &operator=(const Timeline&)=delete;
    void configure(double duration,Transition kind,double now) {
        time(now);if(!std::isfinite(duration))throw std::invalid_argument("Invalid transition duration");
        const auto bounded=std::clamp(duration,200.0,2000.0);
        if(bounded==duration_&&kind==kind_)return;
        duration_=bounded;kind_=kind;
        current_=latest_;pending_.reset();start_.reset();
        scrollStart_=now;decorationElapsed_=0;visibleSince_=now;
    }
    void receive(TrackPtr track,double now) {
        time(now);
        if(track&&!core::validMetadata(*track))throw std::invalid_argument("Invalid timeline track");
        const bool changed=!same(latest_,track);latest_=std::move(track);
        if(!latest_){current_.reset();pending_.reset();start_.reset();return;}
        if(!visible_){return;}
        if(!current_){current_=latest_;scrollStart_=now;return;}
        if(changed){pending_=latest_;if(!start_){start_=now;swapped_=false;}}
        else {
            if(same(current_,latest_))current_=latest_;
            if(pending_)pending_=latest_;
        }
    }
    void visible(bool value,double now) {
        time(now);if(value==visible_)return;
        if(visible_)decorationElapsed_+=now-visibleSince_;
        visible_=value;
        if(value){visibleSince_=now;scrollStart_=now;current_=latest_;pending_.reset();start_.reset();}
    }
    TimelineSample sample(double now) {
        time(now);TimelineSample result;
        result.visible=visible_&&bool(current_);
        result.decorationMilliseconds=decorationElapsed_+(visible_?now-visibleSince_:0);
        if(!result.visible)return result;
        if(start_){
            result.transition=transitionSample(now-*start_,duration_,kind_);
            if(result.transition.incoming&&!swapped_){if(pending_)current_=pending_;pending_.reset();swapped_=true;}
            if(result.transition.finished){start_.reset();scrollStart_=now;if(pending_){start_=now;swapped_=false;}}
        }
        result.track=current_;result.transitioning=bool(start_);
        result.scrollSeconds=start_?0:std::max(0.0,now-scrollStart_)/1000;
        return result;
    }
    bool needsFrames(bool animatedDecoration,bool overflowingText) const {
        owner();return visible_&&bool(current_)&&(bool(start_)||animatedDecoration||overflowingText);
    }
private:
    static bool same(const TrackPtr &a,const TrackPtr &b) {
        return (!a&&!b)||(a&&b&&a->title==b->title&&a->artist==b->artist&&
            (!a->identity||!b->identity||*a->identity==*b->identity));
    }
    void owner() const {if(std::this_thread::get_id()!=owner_)throw std::logic_error("NORMAL timeline owner thread");}
    void time(double now) {
        owner();if(!std::isfinite(now)||now<0||(lastTime_&&now<*lastTime_))throw std::invalid_argument("Non-monotonic timeline clock");lastTime_=now;
    }
    const std::thread::id owner_;
    TrackPtr latest_,current_,pending_;
    std::optional<double> start_,lastTime_;
    double duration_=800,scrollStart_=0,decorationElapsed_=0,visibleSince_=0;
    Transition kind_=Transition::Fade;
    bool visible_=false,swapped_=false;
};
} // namespace playing::visual::normal
