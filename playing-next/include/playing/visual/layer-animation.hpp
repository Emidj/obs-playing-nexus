// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <vector>
#include <cmath>
#include <stdexcept>
#include <algorithm>
namespace playing::visual::animation {
enum class Ease { Linear, Cosine, Step };
enum class Loop { Once, Repeat, PingPong };
enum class Layer : std::size_t { Label, Title, Artist, Artwork, Count };
struct Transform {double x=0,y=0,opacity=1,scale=1,angle=0;};
inline bool same(const Transform &a,const Transform &b) noexcept {return a.x==b.x&&a.y==b.y&&a.opacity==b.opacity&&a.scale==b.scale&&a.angle==b.angle;}
struct Key {double at=0;Transform value;Ease ease=Ease::Linear;};
class Clip final {
public:
    Clip()=default;
    Clip(double duration,double delay,Loop loop,std::vector<Key> keys):duration_(duration),delay_(delay),loop_(loop),keys_(std::move(keys)) {
        if(!std::isfinite(duration)||duration<200||duration>60000||!std::isfinite(delay)||delay<0||delay>60000||keys_.size()<2||keys_.size()>32)throw std::invalid_argument("Animation limits");
        if(loop!=Loop::Once&&loop!=Loop::Repeat&&loop!=Loop::PingPong)throw std::invalid_argument("Animation loop");
        double previous=-1;
        for(const auto &key:keys_) {
            const auto &v=key.value;
            if(!std::isfinite(key.at)||key.at<0||key.at>1||key.at<=previous||!std::isfinite(v.x)||!std::isfinite(v.y)||std::abs(v.x)>4096||std::abs(v.y)>4096||!std::isfinite(v.opacity)||v.opacity<0||v.opacity>1||!std::isfinite(v.scale)||v.scale<.25||v.scale>4||!std::isfinite(v.angle)||std::abs(v.angle)>360)throw std::invalid_argument("Animation key");
            if(key.ease!=Ease::Linear&&key.ease!=Ease::Cosine&&key.ease!=Ease::Step)throw std::invalid_argument("Animation easing");
            previous=key.at;moving_=moving_||!same(v,keys_.front().value);
        }
        if(keys_.front().at!=0||keys_.back().at!=1)throw std::invalid_argument("Animation endpoints");
        if(loop==Loop::Repeat&&!same(keys_.front().value,keys_.back().value))throw std::invalid_argument("Repeat must close seam");
    }
    Transform sample(double elapsed) const {
        if(!std::isfinite(elapsed)||elapsed<0)throw std::invalid_argument("Animation time");
        if(keys_.empty())return {};
        double phase=std::max(0.0,elapsed-delay_)/duration_;
        if(loop_==Loop::Once)phase=std::min(1.0,phase);
        else if(loop_==Loop::Repeat)phase=std::fmod(phase,1.0);
        else {phase=std::fmod(phase,2.0);if(phase>1)phase=2-phase;}
        if(phase>=1)return keys_.back().value;
        for(std::size_t i=1;i<keys_.size();++i)if(phase<keys_[i].at) {
            const auto &a=keys_[i-1],&b=keys_[i];double t=(phase-a.at)/(b.at-a.at);
            if(a.ease==Ease::Cosine)t=(1-std::cos(t*3.141592653589793))/2;
            if(a.ease==Ease::Step)t=0;
            const auto mix=[t](double x,double y){return x+(y-x)*t;};
            return {mix(a.value.x,b.value.x),mix(a.value.y,b.value.y),mix(a.value.opacity,b.value.opacity),mix(a.value.scale,b.value.scale),mix(a.value.angle,b.value.angle)};
        }
        return keys_.back().value;
    }
    bool needsFrames(double elapsed) const {if(!std::isfinite(elapsed)||elapsed<0)throw std::invalid_argument("Animation time");return moving_&&(loop_!=Loop::Once||elapsed<duration_+delay_);}
private:
    double duration_=200,delay_=0;Loop loop_=Loop::Once;std::vector<Key> keys_;bool moving_=false;
};
using Sample=std::array<Transform,static_cast<std::size_t>(Layer::Count)>;
struct Program {
    std::array<Clip,static_cast<std::size_t>(Layer::Count)> clips;
    Sample sample(double time) const {Sample result;for(std::size_t i=0;i<clips.size();++i)result[i]=clips[i].sample(time);return result;}
    bool needsFrames(double time) const {for(const auto &clip:clips)if(clip.needsFrames(time))return true;return false;}
};
class Clock final {
public:
    void visible(bool visible,double now) {check(now);if(visible==visible_)return;if(visible_)elapsed_+=now-start_;visible_=visible;start_=now;}
    double elapsed(double now) {check(now);return elapsed_+(visible_?now-start_:0);}
    void reset(double now) {check(now);elapsed_=0;start_=now;}
private:
    void check(double now){if(!std::isfinite(now)||now<last_)throw std::invalid_argument("Animation clock regression");last_=now;}
    double elapsed_=0,start_=0,last_=0;bool visible_=false;
};
inline double oscillation(double elapsed,double period) noexcept {return (1-std::cos(elapsed*6.283185307179586/period))/2;}
}
