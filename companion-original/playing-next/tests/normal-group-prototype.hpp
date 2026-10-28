// SPDX-License-Identifier: MIT
// Isolated GPU experiment, not linked into the product. The eventual operation
// belongs in the shared renderer; do not ship a parallel NORMAL renderer.
#pragma once
#include <obs.h>
#include <graphics/vec4.h>
#include <QSize>
#include <array>
#include <cmath>
#include <stdexcept>
#include <functional>
namespace playing::test {
class NormalGroupPrototype final {
    struct Slot {
        gs_texrender_t *target = nullptr;
        QSize size;
        uint64_t revision = 0;
        bool valid = false, busy = false;
    };
    std::array<Slot,8> slots_{};
    gs_effect_t *effect_ = nullptr;
    size_t bytes_ = 0;
    uint64_t allocations_ = 0, renders_ = 0, hits_ = 0, draws_ = 0;
public:
    static constexpr size_t budget = 32 * 1024 * 1024;
    ~NormalGroupPrototype() { clear(); }
    NormalGroupPrototype() = default;
    NormalGroupPrototype(const NormalGroupPrototype &) = delete;
    NormalGroupPrototype &operator=(const NormalGroupPrototype &) = delete;
    size_t bytes() const noexcept { return bytes_; }
    uint64_t allocations() const noexcept { return allocations_; }
    uint64_t renders() const noexcept { return renders_; }
    uint64_t hits() const noexcept { return hits_; }
    uint64_t draws() const noexcept { return draws_; }
    void compose(unsigned index, QSize size, uint64_t revision,
                 const std::function<void()> &paint) {
        if (!gs_get_context()) throw std::logic_error("Graphics context required");
        if (index >= slots_.size() || size.width() < 1 || size.height() < 1 ||
            size.width() > 2048 || size.height() > 2048 || !paint)
            throw std::invalid_argument("Invalid group recipe");
        auto &slot = slots_[index];
        if (slot.busy) throw std::logic_error("Recursive group recipe");
        const size_t bytes = size_t(size.width()) * size_t(size.height()) * 4;
        const size_t oldBytes = slot.target ? size_t(slot.size.width()) * size_t(slot.size.height()) * 4 : 0;
        if (bytes_ - oldBytes + bytes > budget) throw std::length_error("Group budget exceeded");
        if (slot.valid && slot.size == size && slot.revision == revision) { ++hits_; return; }
        if (!slot.target) {
            slot.target = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
            if (!slot.target) throw std::runtime_error("Group target allocation failed");
        }
        if (slot.size != size) { bytes_ = bytes_ - oldBytes + bytes; slot.size = size; ++allocations_; }
        slot.valid = false; slot.busy = true;
        struct BusyScope { bool &busy; ~BusyScope() { busy = false; } } busy{slot.busy};
        gs_texrender_reset(slot.target);
        if (!gs_texrender_begin(slot.target, uint32_t(size.width()), uint32_t(size.height())))
            throw std::runtime_error("Group target unavailable");
        struct TargetScope { gs_texrender_t *target; ~TargetScope() { gs_texrender_end(target); } } target{slot.target};
        vec4 empty; vec4_zero(&empty); gs_clear(GS_CLEAR_COLOR,&empty,0,0);
        gs_ortho(0,float(size.width()),0,float(size.height()),-100,100);
        gs_matrix_identity();
        paint();
        slot.revision = revision; slot.valid = true; ++renders_;
    }
    void draw(unsigned index, float x=0, float y=0, float scale=1, float opacity=1) {
        if (!gs_get_context()) throw std::logic_error("Graphics context required");
        if (index >= slots_.size() || !std::isfinite(x) || !std::isfinite(y) ||
            !std::isfinite(scale) || scale <= 0 || scale > 4 ||
            !std::isfinite(opacity) || opacity < 0 || opacity > 1)
            throw std::invalid_argument("Invalid group draw");
        const auto &slot = slots_[index];
        if (!slot.valid || slot.busy) throw std::logic_error("Incomplete group draw");
        if (opacity == 0) return;
        if (!effect_) {
            static constexpr char shader[] = R"(
uniform float4x4 ViewProj;
uniform texture2d image;
uniform float opacity;
sampler_state groupSampler { Filter=Linear; AddressU=Clamp; AddressV=Clamp; };
struct Vertex {float4 pos:POSITION;float2 uv:TEXCOORD0;};
Vertex VS(Vertex v){Vertex o;o.pos=mul(float4(v.pos.xyz,1),ViewProj);o.uv=v.uv;return o;}
float4 PS(Vertex v):TARGET{return image.Sample(groupSampler,v.uv)*opacity;}
technique Draw {pass{vertex_shader=VS(v);pixel_shader=PS(v);}}
)";
            char *error = nullptr; effect_ = gs_effect_create(shader,"normal-group-prototype.effect",&error);
            if (error) bfree(error);
            if (!effect_) throw std::runtime_error("Group shader unavailable");
        }
        const bool srgb = gs_framebuffer_srgb_enabled();
        gs_blend_state_push(); gs_enable_blending(true);
        gs_blend_function(GS_BLEND_ONE,GS_BLEND_INVSRCALPHA);
        gs_enable_framebuffer_srgb(false); gs_matrix_push();
        struct DrawScope { bool srgb; ~DrawScope() { gs_matrix_pop();gs_enable_framebuffer_srgb(srgb);gs_blend_state_pop(); } } restore{srgb};
        gs_matrix_translate3f(x,y,0); gs_matrix_scale3f(scale,scale,1);
        gs_effect_set_texture(gs_effect_get_param_by_name(effect_,"image"),gs_texrender_get_texture(slot.target));
        gs_effect_set_float(gs_effect_get_param_by_name(effect_,"opacity"),opacity);
        while (gs_effect_loop(effect_,"Draw")) gs_draw_sprite(gs_texrender_get_texture(slot.target),0,uint32_t(slot.size.width()),uint32_t(slot.size.height()));
        ++draws_;
    }
    void clear() noexcept {
        obs_enter_graphics();
        for (auto &slot:slots_) { if(slot.target) gs_texrender_destroy(slot.target); slot={}; }
        if(effect_) gs_effect_destroy(effect_); effect_=nullptr;
        bytes_=0; obs_leave_graphics();
    }
};
} // namespace playing::test
