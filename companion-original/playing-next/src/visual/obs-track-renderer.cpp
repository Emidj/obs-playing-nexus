// SPDX-License-Identifier: MIT
#include "playing/visual/obs-track-renderer.hpp"
#include <cmath>
#include <graphics/vec4.h>
namespace playing::visual {
struct UrbanTextTilt {bool active;explicit UrbanTextTilt(bool enabled,QPointF pivot):active(enabled){if(active){gs_matrix_push();gs_matrix_translate3f(float(pivot.x()),float(pivot.y()),0);gs_matrix_rotaa4f(0,0,1,-6*3.14159265f/180);gs_matrix_translate3f(float(-pivot.x()),float(-pivot.y()),0);}}~UrbanTextTilt(){if(active)gs_matrix_pop();}};
static void drawContent(oto::ObsVisualRenderer &renderer,const TrackFrame *frame) {
    auto textMotion=[&](animation::Layer target){
        auto motion=frame->layerMotion[static_cast<std::size_t>(target)];
        if(frame->proArt&&(target==animation::Layer::Title||target==animation::Layer::Artist)){
            motion.opacity*=frame->normalSample.transition.alpha;
            motion.scale*=frame->normalSample.transition.scale;
            motion.y+=frame->canvas.height()*frame->normalSample.transition.offsetYPercent/100.0;
        }
        return motion;
    };
    if(frame->proArt){
        if(!frame->proArt->record&&frame->artwork){oto::DrawParams cover;cover.x=float(frame->artworkPosition.x());cover.y=float(frame->artworkPosition.y());renderer.draw(frame->artwork,true,cover);}
        oto::DrawParams material;
        if(frame->proHue%360){material.sourceRect=frame->proArt->backdrop->image.rect();material.cellHue=float(frame->proHue);material.cellFilter=3;}
        if(frame->proArt->record){auto disk=material;disk.sourceRect=frame->proArt->record->image.rect();disk.x=float(frame->proArt->recordPosition.x());disk.y=float(frame->proArt->recordPosition.y());disk.clip=QRectF(QPointF{},QSizeF(frame->canvas));renderer.draw(frame->proArt->record,true,disk);
            auto reflection=disk;reflection.sourceRect=frame->proArt->recordReflection->image.rect();reflection.opacity=float(frame->recordShimmer);renderer.draw(frame->proArt->recordReflection,reflection.opacity>0,reflection);
            auto ink=disk;ink.sourceRect=frame->proArt->recordInk->image.rect();ink.cellAngle=float(frame->recordAngle);renderer.draw(frame->proArt->recordInk,true,ink);}
        if(frame->proArt->record&&frame->artwork){oto::DrawParams cover;cover.x=float(frame->artworkPosition.x());cover.y=float(frame->artworkPosition.y());renderer.draw(frame->artwork,true,cover);}
        renderer.draw(frame->proArt->backdrop,true,material);
        auto panel=material;panel.opacity=float(frame->panelGain);
        renderer.draw(frame->proArt->panel,panel.opacity>0,panel);
        oto::DrawParams light=material;if(!light.sourceRect.isNull())light.sourceRect=frame->proArt->light->image.rect();light.opacity=float(frame->lightGain)*(frame->proStyle==8?.25f:1.f);
        if(frame->proStyle==14&&light.opacity>0){const double w=frame->canvas.width(),h=frame->canvas.height(),phase=frame->borderSweep*6.28318530718;for(int halo=0;halo<3;++halo){const double angle=phase+halo*3.14159265359;const double x=halo<2?w*.486+std::cos(angle)*w*.285:w*(.50+.23*std::sin(phase)),y=halo<2?h*.42+std::sin(angle)*w*.285:h*(.76+.065*std::cos(phase));for(int ring=4;ring>0;--ring){const double radius=w*(.055+ring*.021);auto liquid=light;liquid.clip=QRectF(x-radius,y-radius,radius*2,radius*2);liquid.opacity*=.18f;renderer.draw(frame->proArt->light,true,liquid);}}}
        else if(frame->proStyle==13&&light.opacity>0&&frame->borderSweep<.65){const double span=frame->canvas.height()*.16,front=(1-frame->borderSweep/.65)*(frame->canvas.height()+span)-span;const float weights[]={.15f,.45f,.9f,.45f,.15f};for(int i=0;i<5;++i){auto paint=material;paint.sourceRect=frame->proArt->spray->image.rect();paint.clip=QRectF(0,front+i*span/5,frame->canvas.width(),span/5);paint.opacity=float(frame->lightGain*weights[i]);renderer.draw(frame->proArt->spray,true,paint);}}
        else if(frame->proStyle==11&&light.opacity>0){const double w=frame->canvas.width(),h=frame->canvas.height(),perimeter=2*(w*.76+h*.78);for(int halo=0;halo<2;++halo){double t=std::fmod(frame->borderSweep+halo*.5,1.0)*perimeter,x,y;if(t<w*.76){x=w*.12+t;y=h*.085;}else if((t-=w*.76)<h*.78){x=w*.88;y=h*.085+t;}else if((t-=h*.78)<w*.76){x=w*.88-t;y=h*.865;}else{t-=w*.76;x=w*.12;y=h*.865-t;}for(int ring=5;ring>0;--ring){auto glass=light;const double radius=w*(.07+ring*.018);glass.clip=QRectF(x-radius,y-radius,radius*2,radius*2);glass.opacity*=.16f;renderer.draw(frame->proArt->light,true,glass);}}}
        else if(frame->proStyle==10&&light.opacity>0){auto ambient=light;ambient.opacity*=.12f;renderer.draw(frame->proArt->light,true,ambient);const double span=frame->canvas.height()*.16,front=(1-frame->borderSweep)*(frame->canvas.height()+span)-span;for(int i=0;i<12;++i){auto grid=light;grid.clip=QRectF(0,front+i*span/12,frame->canvas.width(),span/12);grid.opacity*=float(std::pow(std::sin((i+.5)/12*3.14159265),2));renderer.draw(frame->proArt->light,true,grid);}}
        else if(frame->proStyle==9&&light.opacity>0){const double span=frame->canvas.width()*.24,slice=span/16;for(int side=0;side<2;++side){const double phase=side?1-frame->borderSweep:frame->borderSweep,front=phase*(frame->canvas.width()+span)-span;for(int i=0;i<16;++i){auto liquid=light;liquid.clip=QRectF(front+i*slice,side?frame->canvas.height()*.5:0,slice,frame->canvas.height()*.5);liquid.opacity*=float(.85*std::pow(std::sin((i+.5)/16*3.14159265),2));renderer.draw(frame->proArt->light,true,liquid);}}}
        else if(frame->proStyle==7&&light.opacity>0){const double fill=std::min(1.0,frame->borderSweep/.7);auto bars=light;bars.clip=QRectF(frame->canvas.width()*.315,0,frame->canvas.width()*.245*fill,frame->canvas.height());if(frame->borderSweep>.9)bars.opacity*=float((1-frame->borderSweep)/.1);if(fill>0)renderer.draw(frame->proArt->light,true,bars);}
        else if(frame->proStyle==6&&light.opacity>0){const double span=frame->canvas.width()*.18,slice=span/16;for(int side=0;side<2;++side){const double phase=side?1-frame->borderSweep:frame->borderSweep,front=phase*(frame->canvas.width()+span)-span;for(int i=0;i<16;++i){auto wave=light;wave.clip=QRectF(front+i*slice,side?frame->canvas.height()*.5:0,slice,frame->canvas.height()*.5);const double t=(i+.5)/16;wave.opacity*=float(.9*std::pow(std::sin(t*3.14159265),2));renderer.draw(frame->proArt->light,true,wave);}}}
        else if(frame->proStyle==4&&light.opacity>0){
            auto base=light;base.opacity*=.08f;renderer.draw(frame->proArt->light,true,base);
            const double w=frame->canvas.width(),h=frame->canvas.height();
            const double horizontal=w*.84,vertical=h*.80,perimeter=2*(horizontal+vertical);
            for(int halo=0;halo<2;++halo){double t=std::fmod(frame->borderSweep+halo*.5,1.0)*perimeter,x=0,y=0;
                if(t<horizontal){x=w*.08+t;y=h*.07;}else if((t-=horizontal)<vertical){x=w*.965;y=h*.1+t;}else if((t-=vertical)<horizontal){x=w*.92-t;y=h*.93;}else {t-=horizontal;x=w*.035;y=h*.9-t;}
                for(int ring=5;ring>0;--ring){auto glow=light;const double radius=h*(.06+ring*.014);glow.clip=QRectF(x-radius,y-radius,radius*2,radius*2);glow.opacity*=.12f;renderer.draw(frame->proArt->light,true,glow);}
            }
        }
        else if(frame->proStyle==5&&light.opacity>0&&frame->borderSweep<.58){const double span=frame->canvas.width()*.18,front=frame->borderSweep/.58*(frame->canvas.width()+span)-span;const float weights[]={.15f,.45f,.9f,.45f,.15f};for(int i=0;i<5;++i){auto paint=material;paint.sourceRect=frame->proArt->spray->image.rect();paint.clip=QRectF(front+i*span/5,0,span/5,frame->canvas.height());paint.opacity=float(frame->lightGain*weights[i]);renderer.draw(frame->proArt->spray,true,paint);}}
        else if(frame->proStyle==3&&light.opacity>0){auto ambient=light;ambient.opacity*=.8f;renderer.draw(frame->proArt->light,true,ambient);const float weights[]={.2f,.6f,1.f,.6f,.2f};const double slice=frame->canvas.width()*.14/5;for(int i=0;i<5;++i){auto sheen=light;sheen.clip=QRectF(frame->borderSweep+i*slice,0,slice,frame->canvas.height());sheen.opacity*=weights[i];renderer.draw(frame->proArt->light,true,sheen);}}
        else if(frame->proStyle!=4&&frame->proStyle!=5&&frame->proStyle!=6&&frame->proStyle!=7&&frame->proStyle!=13&&frame->proStyle!=14)renderer.draw(frame->proArt->light,light.opacity>0,light);
        for(size_t i=0;i<frame->proArt->prisms.size();++i){const auto &prism=frame->proArt->prisms[i];const double phase=(frame->borderSweep+i*.23)*6.28318530718,angle=std::sin(phase)*8,radians=angle*3.14159265/180,c=std::cos(radians),s=std::sin(radians),hw=prism.pixels->image.width()/2.0,hh=prism.pixels->image.height()/2.0;const double ex=std::abs(c)*hw+std::abs(s)*hh,ey=std::abs(s)*hw+std::abs(c)*hh;const double cx=std::clamp(prism.position.x()+hw+std::sin(phase)*frame->canvas.width()*.009,ex+1,frame->canvas.width()-ex-1),cy=std::clamp(prism.position.y()+hh+std::cos(phase)*frame->canvas.height()*.022,ey+1,frame->canvas.height()-ey-1);auto shard=material;shard.sourceRect=prism.pixels->image.rect();shard.x=float(cx-(c*hw-s*hh));shard.y=float(cy-(s*hw+c*hh));shard.angle=float(angle);renderer.draw(prism.pixels,true,shard);shard.opacity=float(frame->lightGain*.35);renderer.draw(prism.light,shard.opacity>0,shard);}
    }
    const auto draw=[&](const oto::visual::FramePtr &layer,QPoint position,animation::Layer target) {
        if(!layer) return;
        oto::DrawParams params;params.x=static_cast<float>(position.x());params.y=static_cast<float>(position.y());
        const auto motion=textMotion(target);params.x+=static_cast<float>(motion.x);params.y+=static_cast<float>(motion.y);params.opacity=static_cast<float>(motion.opacity);params.scale=static_cast<float>(motion.scale);params.angle=static_cast<float>(motion.angle);
        if(frame->proArt&&(target==animation::Layer::Title||target==animation::Layer::Artist))params.clip=frame->scrollingViewport[static_cast<std::size_t>(target)-1];
        renderer.draw(layer,true,params);
    };
    {UrbanTextTilt tilt(frame->proStyle==5||frame->proStyle==13,frame->proStyle==13?QPointF(frame->canvas.width()*.5,frame->canvas.height()*.68):QPointF(frame->canvas.width()*.365,frame->canvas.height()*.5));
    if(frame->normalFrame)drawNativeNormal(renderer,frame->normalFrame,frame->normalSample,frame->normalSpeed,frame->normalPause,&frame->layerMotion);
    else {
        draw(frame->title,frame->titlePosition,animation::Layer::Title);draw(frame->artist,frame->artistPosition,animation::Layer::Artist);
        for(std::size_t i=0;i<2;++i)if(frame->scrollingDistance[i]>0){
            const auto motion=textMotion(static_cast<animation::Layer>(i+1));
            auto viewport=QRectF(frame->scrollingViewport[i]).translated(motion.x,motion.y);
            const double scale=std::clamp(motion.scale,.0001,4.0);
            const double offset=normal::scrollOffset(frame->normalSample.scrollSeconds,frame->scrollingDistance[i],frame->normalSpeed,frame->normalPause);
            for(const auto &tile:frame->scrollingText[i]){
                const double x=viewport.center().x()+(tile.x-offset-viewport.width()/2)*scale;
                const double y=viewport.y()+viewport.height()*(1-scale)/2;
                auto clip=viewport.intersected(QRectF(viewport.center().x()+(tile.contentX-offset-viewport.width()/2)*scale,y,tile.contentWidth*scale,viewport.height()*scale));
                if(frame->proArt)clip=clip.intersected(QRectF(frame->scrollingViewport[i]));
                if(clip.isEmpty())continue;
                oto::DrawParams params;params.x=float(x);params.y=float(y);params.scale=float(scale);params.opacity=float(motion.opacity);params.clip=clip;
                renderer.draw(tile.pixels,true,params);
            }
        }
    }
    }
    if(!frame->proArt)draw(frame->artwork,frame->artworkPosition,animation::Layer::Artwork);
}
void drawTrack(oto::ObsVisualRenderer &renderer,const TrackFramePtr &frame,bool active,TransitionSurface *surface) {
    if(!active||!frame)return;
    if(surface&&frame->wholeTransition&&frame->normalSample.transitioning)surface->draw(renderer,frame);
    else drawContent(renderer,frame.get());
}
TransitionSurface::~TransitionSurface(){if(target_)gs_texrender_destroy(target_);if(effect_)gs_effect_destroy(effect_);}
void TransitionSurface::draw(oto::ObsVisualRenderer &renderer,const TrackFramePtr &frame) {
    if(!gs_get_context())throw std::logic_error("Transition graphics context required");
    const auto &sample=frame->normalSample.transition;
    if(sample.alpha<=0)return;
    if(!target_){target_=gs_texrender_create(GS_RGBA,GS_ZS_NONE);if(!target_)throw std::runtime_error("Transition target allocation failed");}
    if(!effect_) {
        const char *shader=R"(
uniform float4x4 ViewProj;uniform texture2d image;uniform float opacity;
sampler_state linearSampler {Filter=Linear;AddressU=Clamp;AddressV=Clamp;};
struct Vertex {float4 pos:POSITION;float2 uv:TEXCOORD0;};
Vertex VS(Vertex v){Vertex o;o.pos=mul(float4(v.pos.xyz,1),ViewProj);o.uv=v.uv;return o;}
float4 PS(Vertex v):TARGET{return image.Sample(linearSampler,v.uv)*opacity;}
technique Draw {pass{vertex_shader=VS(v);pixel_shader=PS(v);}}
)";
        char *error=nullptr;effect_=gs_effect_create(shader,"playing-transition.effect",&error);if(error)bfree(error);
        if(!effect_)throw std::runtime_error("Transition shader unavailable");
    }
    gs_texrender_reset(target_);
    if(!gs_texrender_begin(target_,uint32_t(frame->canvas.width()),uint32_t(frame->canvas.height())))throw std::runtime_error("Transition target unavailable");
    {
        struct End {gs_texrender_t *target;~End(){gs_matrix_pop();gs_texrender_end(target);}} end{target_};
        gs_matrix_push();gs_matrix_identity();gs_ortho(0,float(frame->canvas.width()),0,float(frame->canvas.height()),-100,100);
        vec4 clear;vec4_zero(&clear);gs_clear(GS_CLEAR_COLOR,&clear,0,0);
        auto content=*frame;content.wholeTransition=false;content.normalSample.transition={};content.normalSample.transitioning=false;
        drawContent(renderer,&content);
    }
    const bool srgb=gs_framebuffer_srgb_enabled();gs_enable_framebuffer_srgb(false);
    gs_blend_state_push();gs_enable_blending(true);gs_blend_function(GS_BLEND_ONE,GS_BLEND_INVSRCALPHA);gs_matrix_push();
    struct Restore {bool srgb;~Restore(){gs_matrix_pop();gs_blend_state_pop();gs_enable_framebuffer_srgb(srgb);}} restore{srgb};
    const auto scale=float(sample.scale);
    gs_matrix_translate3f(float(frame->canvas.width())*(1-scale)/2,float(frame->canvas.height())*((1-scale)/2+float(sample.offsetYPercent)/100),0);
    gs_matrix_scale3f(scale,scale,1);
    gs_effect_set_texture(gs_effect_get_param_by_name(effect_,"image"),gs_texrender_get_texture(target_));gs_effect_set_float(gs_effect_get_param_by_name(effect_,"opacity"),float(sample.alpha));
    while(gs_effect_loop(effect_,"Draw"))gs_draw_sprite(gs_texrender_get_texture(target_),0,uint32_t(frame->canvas.width()),uint32_t(frame->canvas.height()));
}
void drawNativeNormal(oto::ObsVisualRenderer &renderer,const normal::PreparedNormalPtr &frame,
    const normal::TimelineSample &sample,double speed,double pause,const animation::Sample *motion) {
    if(!frame||!sample.visible||sample.track!=frame->track)return;
    if(frame->background&&(frame->usePreparedBackground||frame->solidRects.empty()))renderer.draw(frame->background,true,{});
    for(const auto &rectangle:frame->solidRects) {
        if(frame->usePreparedBackground&&!rectangle.travel)continue;
        auto box=rectangle.box;
        if(rectangle.travel)box.translate(rectangle.travel*animation::oscillation(sample.decorationMilliseconds,rectangle.period),0);
        renderer.drawSolidRect(box,rectangle.color);
    }
    for(const auto &layer:frame->ornaments) {
        const double wave=animation::oscillation(sample.decorationMilliseconds,layer.period);
        oto::DrawParams params;params.x=static_cast<float>(layer.position.x()+wave*layer.travel.x());params.y=static_cast<float>(layer.position.y()+wave*layer.travel.y());
        params.opacity=static_cast<float>(1-layer.pulse+layer.pulse*wave);
        renderer.draw(layer.pixels,true,params);
    }
    for(std::size_t i=0;i<frame->text.size();++i) {
        const auto &row=frame->layout.rows[i];
        const auto transform=motion?(*motion)[i]:animation::Transform{};
        const double alpha=(i?sample.transition.alpha:1)*transform.opacity;
        if(alpha<=0)continue;
        const double zoom=std::clamp((i?sample.transition.scale:1)*transform.scale,.0001,4.0);
        const double y=row.box.y()+(i?sample.transition.offsetYPercent*row.box.height()/100:0)+row.box.height()*(1-zoom)/2+transform.y;
        const double offset=normal::scrollOffset(sample.scrollSeconds,row.overflow,speed,pause);
        const auto viewport=row.box.translated(transform.x,transform.y);
        for(const auto &tile:frame->text[i]) {
            const double x=row.box.center().x()+(tile.x-offset-row.box.width()/2)*zoom+transform.x;
            const QRectF bounds(x,y,tile.pixels->image.width()*zoom,tile.pixels->image.height()*zoom);
            if(!bounds.intersects(viewport))continue;
            const QRectF content(x+(tile.contentX-tile.x)*zoom,y,tile.contentWidth*zoom,bounds.height());
            const auto clip=viewport.intersected(content);
            if(clip.isEmpty())continue;
            oto::DrawParams params;params.x=static_cast<float>(x);params.y=static_cast<float>(y);
            params.angle=static_cast<float>(transform.angle);
            params.scale=static_cast<float>(zoom);params.opacity=static_cast<float>(alpha);params.clip=clip;
            params.legacyAlpha=frame->legacyTextBlend&&tile.pixels->encoding==oto::visual::RasterEncoding::StraightSrgb&&row.overflow==0&&!(i&&sample.transitioning);
            renderer.draw(tile.pixels,true,params);
        }
    }
}
}
