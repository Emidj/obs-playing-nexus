// SPDX-License-Identifier: MIT
#include "playing/visual/track-visual.hpp"
#include "playing/visual/text-only-layout.hpp"
#include "playing/visual/vertical-pro-geometry.hpp"
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QPainter>
#include "visual/asset-manager.hpp"
#include <iostream>
#include <thread>
#include <stdexcept>
using namespace playing;
namespace {
unsigned checks=0;
void check(bool ok,const char *why) {++checks;if(!ok)throw std::runtime_error(why);}
template<class F> void rejects(F fn) {bool failed=false;try{fn();}catch(const std::invalid_argument &){failed=true;}check(failed,"Invalid geometry/style accepted");}
}
int main(int argc,char **argv) {
    QGuiApplication app(argc,argv);
    try {
        visual::TrackVisualHost host; visual::TrackLayout layout;
        {
            visual::TrackLayout geometry;geometry.canvas=QSize(400,720);geometry.title.box=QRect(90,420,220,70);geometry.artist.box=QRect(90,502,220,40);geometry.artworkBox=QRect(80,160,240,240);
            const auto identity=visual::offsetTextBlock(geometry,0,0);check(identity.title.box==geometry.title.box&&identity.artist.box==geometry.artist.box,"Zero sliders change original text geometry");
            for(const auto delta:{QPoint(-35,-25),QPoint(35,25),QPoint(-2048,-2048),QPoint(2048,2048)}){
                const auto moved=visual::offsetTextBlock(geometry,delta.x(),delta.y());
                check(moved.title.box.topLeft()-geometry.title.box.topLeft()==moved.artist.box.topLeft()-geometry.artist.box.topLeft(),"Sliders do not move title and artist together");
                check(moved.artworkBox==geometry.artworkBox&&QRect(QPoint{},geometry.canvas).contains(moved.title.box.united(moved.artist.box)),"Text offset moved artwork or escaped source canvas");
            }
        }

        for(const auto canvas:{QSize(400,720),QSize(480,720),QSize(800,1440)})for(int style:{10,11,12})for(int requested:{16,100,512}){
            const auto aperture=visual::verticalCoverAperture(style,canvas);
            const auto cover=visual::centredVerticalCover(style,canvas,requested);
            check(std::abs(QRectF(cover).center().x()-aperture.center().x())<=.51&&std::abs(QRectF(cover).center().y()-aperture.center().y())<=.51,"Resizing album shifted its centre inside the frame");
            check(cover.width()==cover.height()&&QRectF(cover).adjusted(1,1,-1,-1).intersected(aperture)==QRectF(cover).adjusted(1,1,-1,-1),"Album escapes the reference aperture");
        }

        {
            QImage image(20,20,QImage::Format_RGBA8888_Premultiplied);image.fill(Qt::transparent);image.setPixelColor(8,8,QColor(80,160,240,128));
            check(visual::applyTextEffects(image,{})==image,"Zero text effects changed pixels");
            visual::TextEffects effect;effect.outline=2;effect.shadow=3;effect.outlineColor=QColor(255,0,0,128);
            const auto output=visual::applyTextEffects(image,effect);check(output.pixelColor(6,8).alpha()>0&&output.pixelColor(11,11).alpha()>0,"Outline/shadow masks missing");
            bool premultiplied=true;for(int y=0;y<output.height();++y)for(int x=0;x<output.width();++x){const auto *p=output.constScanLine(y)+x*4;premultiplied=premultiplied&&p[0]<=p[3]&&p[1]<=p[3]&&p[2]<=p[3];}check(premultiplied,"Text effects broke alpha premultiplication");
            effect.outline=9;rejects([&]{visual::applyTextEffects(image,effect);});
        }
        for(const auto size:{QSize(64,64),QSize(640,120),QSize(320,360),QSize(2048,2048)})for(bool vertical:{false,true})for(bool cover:{false,true})for(int margin:{0,12,128}) {
            auto automatic=layout;automatic.canvas=size;automatic.artworkEnabled=cover;
            automatic=visual::textOnlyLayout(automatic,vertical,margin,128,128,512);
            const QRect bounds(QPoint{},size);
            check(automatic.title.box.isValid()&&automatic.artist.box.isValid()&&bounds.contains(automatic.title.box)&&bounds.contains(automatic.artist.box),"Text-only extreme bounds");
            check(!automatic.title.box.intersects(automatic.artist.box),"Text-only text overlap");
            if(cover)check(bounds.contains(automatic.artworkBox)&&!automatic.artworkBox.intersects(automatic.title.box)&&!automatic.artworkBox.intersects(automatic.artist.box),"Text-only artwork overlaps text");
        }
        auto automatic=layout;automatic.artworkEnabled=true;
        const auto withCover=visual::textOnlyLayout(automatic,false,12,12,16,100);automatic.artworkEnabled=false;
        const auto withoutCover=visual::textOnlyLayout(automatic,false,12,12,16,100);
        check(withoutCover.title.box.width()>withCover.title.box.width()&&withoutCover.title.box.x()<withCover.title.box.x(),"Disabled cover reserves space");
        layout.title.box=QRect(12,8,600,46);layout.artist.box=QRect(12,65,600,38);
        layout.title.pixelSize=30;layout.artist.pixelSize=22;layout.title.overflow=oto::visual::Overflow::Ellipsis;
        core::PlayingCore core;core.start();
        check(!host.prepare(core.snapshot(),layout),"Waiting rendered");check(host.stats().renders==0,"Waiting did work");
        core.observe({core::Playback::Playing,core::Metadata{u8"Jóga 🎵 日本語",u8"Björk"}});
        {
            visual::TrackVisualHost centredHost;auto centredLayout=layout;centredLayout.proPreset=12;centredLayout.canvas=QSize(400,720);centredLayout.title.box=QRect(88,450,224,100);centredLayout.artist.box=QRect(88,562,224,60);centredLayout.title.horizontal=centredLayout.artist.horizontal=oto::visual::Horizontal::Center;centredLayout.title.family="Reigo ExtBd";centredLayout.artist.family="Reigo Light";
            for(const auto text:{"jazz","WWW","Sun (Extended Mix)"}){core::PlayingCore song;song.start();song.observe({core::Playback::Playing,core::Metadata{text,text}});const auto centred=centredHost.prepare(song.snapshot(),centredLayout);
                for(const auto &layer:{centred->title,centred->artist}){int left=layer->image.width(),right=-1;for(int y=0;y<layer->image.height();++y)for(int x=0;x<layer->image.width();++x)if(layer->image.constScanLine(y)[x*4+3]>32){left=std::min(left,x);right=std::max(right,x);}check(right>=left&&std::abs((left+right+1)/2.-layer->image.width()/2.)<=1.5,"Vertical Vinyl visible text is not centred within its box");}
            }
        }
        const auto first=host.prepare(core.snapshot(),layout);check(first&&first->title&&first->artist,"Track layers absent");
        check(first->snapshot==core.snapshot(),"Original snapshot not retained");
        check(first->titlePosition==QPoint(12,8)&&first->artistPosition==QPoint(12,65),"Layer positions lost");
        check(host.stats().renders==2,"Expected two preparations");
        const auto second=host.prepare(core.snapshot(),layout);check(first->title==second->title&&first->artist==second->artist,"Consumers did not share frames");
        check(host.stats().renders==2&&host.stats().hits==2,"Identical state rasterized again");
        {visual::TrackVisualHost effectsHost;const auto base=effectsHost.prepare(core.snapshot(),layout);auto decorated=layout;decorated.textEffects[0].outline=2;decorated.textEffects[0].shadow=3;const auto frame=effectsHost.prepare(core.snapshot(),decorated);check(frame->title!=base->title&&frame->artist==base->artist,"Text effects changed wrong layer");check(frame->title->image.size()==base->title->image.size(),"Effects expanded consumer canvas");}
        {
            visual::TrackVisualHost variants;
            for(int style=1;style<=14;++style){auto base=layout;base.proPreset=style;base.canvas=style>=10?QSize(480,720):QSize(960,367);base.title.box=QRect(200,40,250,55);base.artist.box=QRect(200,110,250,45);const auto before=variants.prepare(core.snapshot(),base);
                auto custom=base;custom.title.family="Arial";custom.title.color=QColor(255,220,0,128);custom.title.pixelSize=32;custom.textEffects[0].outline=2;custom.textEffects[0].shadow=3;const auto after=variants.prepare(core.snapshot(),custom);
                check(after->proArt==before->proArt&&after->artist==before->artist,"PRO text customization altered style material or artist");check(after->title->image!=before->title->image,"PRO text customization not visible");
            }
        }
        {
            auto pro=layout;pro.proPreset=1;pro.proLightIntensity=0;
            visual::TrackVisualHost pilot;
            const auto off=pilot.prepare(core.snapshot(),pro);
            check(off->proArt&&off->lightGain==0,"PRO light not separate/disabled");
            const auto rasters=pilot.stats().renders;pro.proLightIntensity=100;
            const auto on=pilot.prepare(core.snapshot(),pro);
            check(on->proArt==off->proArt&&on->lightGain==1&&pilot.stats().renders==rasters,"Intensity rebuilt graphics");
            pro.proHue=360;const auto original=pilot.prepare(core.snapshot(),pro);
            check(original->proArt==off->proArt,"Hue360 differs from original");
            pro.proHue=120;const auto hue=pilot.prepare(core.snapshot(),pro);
            check(hue->proArt==off->proArt&&hue->proHue==120&&hue->title==off->title&&hue->artist==off->artist,"GPU hue rebuilt pixels/altered text");
            check(pilot.stats().renders==rasters,"GPU hue rerasterized");
            const auto &a=hue->proArt->light->image;const auto &b=off->proArt->light->image;bool alpha=true;
            for(int y=0;y<a.height();++y)for(int x=0;x<a.width();++x)alpha=alpha&&a.pixelColor(x,y).alpha()==b.pixelColor(x,y).alpha();
            check(alpha,"Hue changed alpha mask");
            const QImage authored=QImage(":/playing/pro-assets/neon-club-reference-v1.png").scaled(pro.canvas,Qt::IgnoreAspectRatio,Qt::SmoothTransformation).convertToFormat(QImage::Format_RGBA8888_Premultiplied);
            QImage combined=off->proArt->backdrop->image.copy();
            {QPainter painter(&combined);painter.drawImage(0,0,off->proArt->panel->image);}
            int difference=0;for(int y=0;y<combined.height();++y)for(int x=0;x<combined.width()*4;++x)difference=std::max(difference,std::abs(int(combined.constScanLine(y)[x])-int(authored.constScanLine(y)[x])));
            check(difference==0,"Zero light altered the authored frame");
            for(const auto p:{QPointF(750,260),QPointF(900,250),QPointF(1300,400),QPointF(1820,400),QPointF(900,600)}){const QPoint sample(qRound(p.x()*pro.canvas.width()/2030),qRound(p.y()*pro.canvas.height()/775));check(off->proArt->backdrop->image.pixelColor(sample).alpha()==0&&off->proArt->panel->image.pixelColor(sample).alpha()>0,"Neon opacity leaves part of its interior attached to the frame");}
            for(const auto p:{QPointF(1000,210),QPointF(1910,400),QPointF(1600,615),QPointF(210,300)}){const QPoint sample(qRound(p.x()*pro.canvas.width()/2030),qRound(p.y()*pro.canvas.height()/775));check(off->proArt->panel->image.pixelColor(sample).alpha()==0,"Neon background opacity removes a frame rail");}
            pro.proPanelOpacity=0;const auto transparent=pilot.prepare(core.snapshot(),pro);
            check(transparent->panelGain==0&&transparent->proArt==off->proArt&&pilot.stats().renders==rasters,"Panel opacity rebuilt material");
            check(off->proArt->panel->image.pixelColor(pro.canvas.width()*6/10,pro.canvas.height()/2).alpha()>0&&off->proArt->backdrop->image.pixelColor(pro.canvas.width()*6/10,pro.canvas.height()/2).alpha()==0,"Panel not independently transparent");
            pro.proHue=0;core::PlayingCore nextCore;nextCore.start();nextCore.observe({core::Playback::Playing,core::Metadata{"Next PRO song","Next artist"}});
            const auto before=pilot.stats().renders;const auto next=pilot.prepare(nextCore.snapshot(),pro);
            check(next->proArt==off->proArt,"Track change rebuilt static PRO decoration");
            check(pilot.stats().renders==before+2,"Track change rasterized more than title and artist");
            pro.proPreset=2;const auto vinyl=pilot.prepare(nextCore.snapshot(),pro);
            check(vinyl->proArt!=off->proArt&&vinyl->proArt->record,"Vinyl lacks independent record material");
            const auto vinylRasters=pilot.stats().renders;pro.proHue=180;pro.proPanelOpacity=45;
            const auto adjusted=pilot.prepare(nextCore.snapshot(),pro);
            check(adjusted->proArt==vinyl->proArt&&adjusted->panelGain==.45&&pilot.stats().renders==vinylRasters,"Vinyl controls rebuilt layers");
            auto aligned=pro;aligned.canvas=QSize(960,367);aligned.artworkBox=QRect(57,82,192,192);
            const auto alignedFrame=pilot.prepare(nextCore.snapshot(),aligned);
            check(alignedFrame->proArt->recordPosition==QPoint(165,44)&&alignedFrame->proArt->record->image.height()==271,"Vinyl does not match selected circular aperture");
            aligned.artworkBox.moveTop(90);const auto movedFrame=pilot.prepare(nextCore.snapshot(),aligned);
            check(movedFrame->proArt->recordPosition==alignedFrame->proArt->recordPosition,"Cover spacing moved the fixed vinyl aperture");
            pro.title.family="Segoe Script";pro.title.italic=true;pro.title.pixelSize=128;
            const auto script=pilot.prepare(nextCore.snapshot(),pro);bool touchesEdge=false;
            for(int x=0;x<script->title->image.width();++x)touchesEdge=touchesEdge||script->title->image.pixelColor(x,0).alpha()!=0||script->title->image.pixelColor(x,script->title->image.height()-1).alpha()!=0;
            check(script->title->textMetrics[0].pixelSize<128&&!touchesEdge,"Tall italic font clipped vertical glyphs");
            pro.proPreset=3;const auto minimal=pilot.prepare(nextCore.snapshot(),pro);
            check(minimal->proStyle==3&&minimal->proArt!=vinyl->proArt&&!minimal->proArt->record,"Minimal reused Vinyl material or record");
            const auto minimalRasters=pilot.stats().renders;pro.proHue=240;const auto tinted=pilot.prepare(nextCore.snapshot(),pro);
            check(tinted->proArt==minimal->proArt&&pilot.stats().renders==minimalRasters,"Minimal palette regenerated material");
            pro.proPreset=5;const auto urban=pilot.prepare(nextCore.snapshot(),pro);check(bool(urban->proArt->spray)&&urban->proArt->spray->image.size()==pro.canvas,"Urban paint accent does not follow the canvas material");
            const auto urbanOriginal=QImage(":/playing/pro-assets/urban-reference-v1.png").scaled(pro.canvas,Qt::IgnoreAspectRatio,Qt::SmoothTransformation).convertToFormat(QImage::Format_RGBA8888);int inventedPaint=0;for(int y=0;y<urbanOriginal.height();++y)for(int x=0;x<urbanOriginal.width();++x)if(urban->proArt->spray->image.pixelColor(x,y).alpha()>urbanOriginal.pixelColor(x,y).alpha())++inventedPaint;check(inventedPaint==0,"Urban animation introduces shapes outside original paint");
            int retainedBlack=0;const auto &urbanBackdrop=urban->proArt->backdrop->image;for(int y=0;y<urbanBackdrop.height();++y)for(int x=0;x<urbanBackdrop.width();++x){const auto c=urbanBackdrop.pixelColor(x,y);if(c.alpha()>32&&c.valueF()<.30&&c.saturationF()<.35)++retainedBlack;}
            check(retainedBlack==0,"Urban black paint remains outside opacity layer");
            pro.canvas=QSize(960,320);pro.title.box=QRect(350,50,500,100);pro.artist.box=QRect(350,150,500,90);pro.title.pixelSize=48;pro.artist.pixelSize=36;pro.titlePingPong=pro.artistPingPong=false;
            const auto visibleGap=[](const visual::TrackFramePtr &frame){int lastTitle=-1,firstArtist=frame->artist->image.height();for(int y=0;y<frame->title->image.height();++y)for(int x=0;x<frame->title->image.width();++x)if(frame->title->image.pixelColor(x,y).alpha())lastTitle=std::max(lastTitle,y);for(int y=0;y<frame->artist->image.height();++y)for(int x=0;x<frame->artist->image.width();++x)if(frame->artist->image.pixelColor(x,y).alpha())firstArtist=std::min(firstArtist,y);return frame->artistPosition.y()+firstArtist-frame->titlePosition.y()-lastTitle-1;};
            check(visibleGap(pilot.prepare(nextCore.snapshot(),pro))==0,"Zero gap retains invisible font padding");pro.artist.box.moveTop(160);check(visibleGap(pilot.prepare(nextCore.snapshot(),pro))==10,"Visible positive gap is inaccurate");pro.artist.box.moveTop(140);check(visibleGap(pilot.prepare(nextCore.snapshot(),pro))==-10,"Negative gap does not overlap visible glyphs");
            pro.proPreset=6;const auto wave=pilot.prepare(nextCore.snapshot(),pro);check(wave->proStyle==6&&!wave->proArt->record&&!wave->proArt->spray,"Waveform reused another style decoration");check(wave->proArt->panel->image.pixelColor(600,160).alpha()==255&&wave->proArt->panel->image.pixelColor(600,10).alpha()==0,"Waveform interior contour missing");check(wave->proArt->light->image.pixelColor(600,160).alpha()==0,"Waveform light fills the text field");
            pro.proPreset=7;const auto hud=pilot.prepare(nextCore.snapshot(),pro);check(hud->proStyle==7&&hud->proArt!=wave->proArt&&!hud->proArt->record&&!hud->proArt->spray,"HUD reused another style material");check(hud->proArt->panel->image.pixelColor(600,160).alpha()==255&&hud->proArt->panel->image.pixelColor(880,65).alpha()==0,"HUD chamfered interior missing");check(hud->proArt->light->image.pixelColor(600,160).alpha()==0,"HUD scan fills text field");
            int topBar=0,bottomBar=0,otherLight=0;const auto &bars=hud->proArt->light->image;for(int y=0;y<bars.height();++y)for(int x=0;x<bars.width();++x)if(bars.pixelColor(x,y).alpha()){const double nx=(x+.5)/bars.width(),ny=(y+.5)/bars.height();if(nx>=.315&&nx<=.56&&ny>.10&&ny<.15)++topBar;else if(nx>=.315&&nx<=.56&&ny>.84&&ny<.89)++bottomBar;else ++otherLight;}check(topBar>50&&bottomBar>50&&otherLight==0,"HUD fill must affect only its two original bars");
            pro.proPreset=8;const auto particles=pilot.prepare(nextCore.snapshot(),pro);check(particles->proStyle==8&&particles->proArt->prisms.size()==3,"Particles must contain three independent source prisms");for(const auto &prism:particles->proArt->prisms)check(prism.pixels&&prism.light&&prism.pixels->image.size()==prism.light->image.size()&&prism.position.x()>350,"Particles prism overlaps album area or lacks light material");check(particles->proArt->panel->image.pixelColor(600,160).alpha()>240,"Particles dark material not separated for opacity");
        }
        const auto stats=host.stats();check(!host.prepare(core.snapshot(),layout,false),"Inactive rendered");
        check(host.stats().renders==stats.renders&&host.stats().hits==stats.hits,"Inactive touched cache");
        core.observe({core::Playback::Playing,core::Metadata{u8"Jóga 🎵 日本語","Other artist"}});
        const auto changed=host.prepare(core.snapshot(),layout);check(changed->title==first->title&&changed->artist!=first->artist,"Unchanged title not reused");
        check(host.stats().renders==3,"Artist change rebuilt title");
        auto moved=layout;moved.title.box.moveLeft(20);const auto movedFrame=host.prepare(core.snapshot(),moved);
        check(movedFrame->title==changed->title&&movedFrame->titlePosition.x()==20,"Position-only change rasterized");
        auto colored=layout;colored.title.color=Qt::cyan;const auto colorFrame=host.prepare(core.snapshot(),colored);
        check(colorFrame->title!=changed->title&&colorFrame->artist==changed->artist,"Color invalidation not isolated");
        {
            auto vertical=layout;vertical.proPreset=12;vertical.canvas=QSize(400,720);vertical.artworkEnabled=true;
            vertical.artworkBox=visual::centredVerticalCover(12,vertical.canvas,512);
            vertical.title.box=QRect(95,450,210,70);vertical.artist.box=QRect(95,530,210,50);
            const auto frame=host.prepare(core.snapshot(),vertical);
            const auto centre=visual::verticalCoverAperture(12,vertical.canvas).center().toPoint();
            check(frame->proArt->backdrop->image.pixelColor(centre).alpha()==0&&frame->proArt->panel->image.pixelColor(centre).alpha()==0&&frame->proArt->light->image.pixelColor(centre).alpha()==0,"Vinyl body/panel covers the album aperture");
            int gold=0,dark=0;bool lostLight=false;
            for(int y=0;y<720;++y)for(int x=0;x<400;++x){
                const auto panel=frame->proArt->panel->image.pixelColor(x,y);
                const auto border=frame->proArt->backdrop->image.pixelColor(x,y);
                if(panel.alpha()>0)++dark;
                if(border.alpha()>128&&border.valueF()>.5&&border.saturationF()>.4)++gold;
                lostLight=lostLight||(panel.alpha()>128&&panel.valueF()>.45);
            }
            check(!lostLight,"Vertical Vinyl opacity removes a luminous frame detail");
            check(gold>1500&&dark>10000,"Vertical Vinyl lost border or controllable dark background");
            if(argc>1&&std::string(argv[1])=="--vertical-audit"){
                QImage pixels(vertical.canvas,QImage::Format_RGBA8888_Premultiplied);pixels.fill(Qt::transparent);
                {QPainter p(&pixels);p.drawImage(frame->proArt->recordPosition,frame->proArt->record->image);p.drawImage(frame->proArt->backdrop->image.rect(),frame->proArt->backdrop->image);}
                pixels.save("vertical-vinyl-opacity-zero.png");
                {QPainter p(&pixels);p.drawImage(frame->proArt->panel->image.rect(),frame->proArt->panel->image);}
                pixels.save("vertical-vinyl-opacity-full.png");
            }
        }

        auto invalid=layout;invalid.title.box.moveLeft(-1);rejects([&]{host.prepare(core.snapshot(),invalid);});
        core.observe({core::Playback::Playing,core::Metadata{u8"Long Unicode 日本語 🎵 title with enough words to overflow the narrow text box", "Other artist"}});
        auto narrow=layout;narrow.title.box.setWidth(160);
        oto::visual::TextLayoutEngine textEngine;
        auto node=narrow.title;node.text=QString::fromUtf8(core.snapshot()->track->title.c_str());
        node.overflow=oto::visual::Overflow::AutoFit;
        const auto fit=textEngine.prepare(node);
        check(fit.metrics.pixelSize>=node.minimumPixelSize&&fit.metrics.pixelSize<node.pixelSize,"AutoFit minimum/search boundary");
        node.overflow=oto::visual::Overflow::Ellipsis;
        check(textEngine.prepare(node).metrics.elided,"Long Unicode not elided");
        node.overflow=oto::visual::Overflow::Wrap;
        check(textEngine.prepare(node).metrics.lines>1,"Long Unicode did not wrap");
        node.overflow=oto::visual::Overflow::Clip;
        check(textEngine.prepare(node).metrics.overflow,"Clip lost overflow diagnostic");
        {
            auto ping=narrow;ping.titlePingPong=true;ping.title.overflow=oto::visual::Overflow::Clip;
            const auto moving=host.prepare(core.snapshot(),ping);
            check(moving->scrollingDistance[0]>0&&!moving->title&&!moving->scrollingText[0].empty(),"Ping-pong did not retain complete text");
            check(moving->scrollingViewport[0]==ping.title.box,"Ping-pong viewport lost");
            const auto renders=host.stats().renders;const auto same=host.prepare(core.snapshot(),ping);
            check(host.stats().renders==renders&&same->scrollingText[0][0].pixels==moving->scrollingText[0][0].pixels,"Ping-pong repeated rasterization");
            const double distance=moving->scrollingDistance[0],travel=distance/40;
            check(visual::normal::scrollOffset(0,distance,40,2)==0&&std::abs(visual::normal::scrollOffset(2+travel,distance,40,2)-distance)<.001&&visual::normal::scrollOffset(4+2*travel,distance,40,2)<.001,"Ping-pong endpoints");
        }
        oto::visual::FramePtr prior;
        for(int mode=0;mode<4;++mode){
            narrow.title.overflow=static_cast<oto::visual::Overflow>(mode);
            const auto frame=host.prepare(core.snapshot(),narrow);
            check(frame->title->image.size()==QSize(160,46),"Overflow escaped title box");
            check(!prior||frame->title!=prior,"Overflow mode reused incompatible frame");
            const auto same=host.prepare(core.snapshot(),narrow);
            check(same->title==frame->title&&same->artist==frame->artist,"Overflow repeated rasterization");prior=frame->title;
        }
        invalid=layout;invalid.canvas=QSize(2049,120);rejects([&]{host.prepare(core.snapshot(),invalid);});
        invalid=layout;invalid.title.pixelSize=200;rejects([&]{host.prepare(core.snapshot(),invalid);});
        visual::TrackPublication publication;publication.publish(first);bool read=false,rejected=false;
        std::thread other([&]{read=publication.read()==first;try{host.prepare(core.snapshot(),layout);}catch(const std::logic_error &){rejected=true;}});other.join();
        check(read&&rejected,"Thread handoff/GUI ownership failed");publication.publish({});check(!publication.read(),"Publication did not clear");
        auto nativeLayout=layout;nativeLayout.normalPreset=1;nativeLayout.canvas.setWidth(720);
        const auto nativeFrame=host.prepare(core.snapshot(),nativeLayout);
        check(nativeFrame->normalFrame&&nativeFrame->normalSample.visible&&nativeFrame->normalSample.track==nativeFrame->normalFrame->track,"Standalone NORMAL frame lacks immutable visible sample");
        check(host.prepare(core.snapshot(),nativeLayout)->normalFrame==nativeFrame->normalFrame,"Standalone NORMAL repeats font preparation");
        {visual::TrackVisualHost decoratedHost;const auto plain=decoratedHost.prepare(core.snapshot(),layout);
         auto under=layout;under.textEffects[0].underline=true;const auto u=decoratedHost.prepare(core.snapshot(),under);
         check(u->title->image!=plain->title->image&&u->artist==plain->artist,"Underline must alter only title pixels");
         auto strike=layout;strike.textEffects[1].strikeout=true;const auto s=decoratedHost.prepare(core.snapshot(),strike);
         check(s->artist->image!=plain->artist->image&&s->title==plain->title,"Strikeout must alter only artist pixels");
         check(decoratedHost.prepare(core.snapshot(),layout)->title==plain->title,"Decoration off must restore cached plain raster");
         auto pro=layout;pro.proPreset=1;const auto idle=decoratedHost.prepareIdle(pro);
         check(idle&&idle->proArt&&!idle->title&&!idle->artist&&!idle->artwork,"Idle PRO must retain frame without track content");}
        core.observe({core::Playback::Paused,std::nullopt});check(!host.prepare(core.snapshot(),layout),"Pause showed retained song");
        core.stop();check(!host.prepare(core.snapshot(),layout),"Stopped rendered");
        host.clear();check(host.stats().entries==0&&host.stats().bytes==0,"Host cache did not clear");
        check(first->snapshot->track->title==u8"Jóga 🎵 日本語"&&!first->title->image.isNull(),"Retained immutable state invalidated");
        QTemporaryDir assets;check(assets.isValid(),"Asset fixture directory");
        QImage png(32,32,QImage::Format_RGBA8888_Premultiplied);png.fill(Qt::cyan);
        check(png.save(assets.filePath("one.png"))&&png.save(assets.filePath("two.png")),"PNG fixtures");
        oto::visual::AssetManager manager(assets.path(),1,8192);
        check(manager.stats().loads==0,"Asset manager not lazy");
        const auto asset=manager.load("one.png");check(asset&&asset->image.pixelColor(0,0)==QColor(Qt::cyan),"PNG decode");
        check(manager.load("one.png")==asset&&manager.stats().loads==1&&manager.stats().hits==1,"Asset consumers not shared");
        check(!oto::visual::AssetManager::validReference("../one.png")&&!oto::visual::AssetManager::validReference("one.svg"),"Unsupported/traversal reference accepted");
        rejects([&]{manager.load("../one.png");});
        manager.load("two.png");check(manager.stats().entries==1&&manager.stats().evictions==1,"Asset LRU budget");
        const auto circle=host.prepareArtwork(asset,QSize(64,64),true,true);check(circle->image.pixelColor(0,0).alpha()==0&&circle->image.pixelColor(32,32)==QColor(Qt::cyan),"Circular cover mask must clear corners and preserve center");check(asset->image.pixelColor(0,0)==QColor(Qt::cyan),"Circular masking mutated shared decoded artwork");
        manager.clear();check(manager.stats().entries==0&&asset->image.pixelColor(0,0)==QColor(Qt::cyan),"Asset clear invalidated retained image");
        for(const auto dimensions:{QSize(400,200),QSize(200,400),QSize(123,123),QSize(83,116)}){
            auto image=std::make_shared<oto::visual::ImageData>();image->image=QImage(dimensions,QImage::Format_RGBA8888_Premultiplied);image->image.fill(Qt::red);
            {QPainter painter(&image->image);const int side=std::min(dimensions.width(),dimensions.height())/3;painter.fillRect(QRect((dimensions.width()-side)/2,(dimensions.height()-side)/2,side,side),Qt::white);}
            const auto centred=host.prepareArtwork(image,QSize(64,64),true);
            check(centred->image.pixelColor(32,32)==QColor(Qt::white),"Non-square artwork lost its original centre");
            check(centred->image.pixelColor(0,0).alpha()==255&&centred->image.pixelColor(63,63).alpha()==255,"Fill artwork leaves asymmetric empty margins");
        }
        const char* referenceNames[]={"neon-club-reference-v1.png","vinyl-classic-thin-aligned-v5.png","minimal-reference-v1.png","glassmorph-reference-v1.png","urban-reference-v1.png","waveform-reference-v1.png","hud-reference-v1.png","particles-reference-v1.png","liquid-reference-v1.png","vertical-retro-reference-v3.png","vertical-glass-reference-v1.png","vinyl-classic-thin-aligned-v5.png","vertical-urban-reference-v1.png","vertical-liquid-reference-v2.png"};
        for(int style=1;style<=14;++style)for(const auto dimensions:{QSize(400,200),QSize(200,400),QSize(83,116)}){
            auto pro=layout;pro.proPreset=style;pro.canvas=style>=10?QSize(style==12?400:style==13?384:480,720):QSize(960,style<=3?367:style<=5?280:320);
            const auto geometry=visual::proCoverGeometry(style,pro.canvas);pro.artworkBox=geometry.box;
            auto image=std::make_shared<oto::visual::ImageData>();image->image=QImage(dimensions,QImage::Format_RGBA8888_Premultiplied);image->image.fill(QColor(0,255,0));
            const auto cover=host.prepareProArtwork(image,pro);
            QImage composite(pro.canvas,QImage::Format_RGBA8888_Premultiplied);composite.fill(Qt::transparent);{QPainter p(&composite);p.drawImage(geometry.box.topLeft(),cover->image);}
            auto source=QImage(QStringLiteral(":/playing/pro-assets/")+referenceNames[style-1]).convertToFormat(QImage::Format_RGBA8888);
            if(style==3||style==4){QRect crop;for(int y=0;y<source.height();++y)for(int x=0;x<source.width();++x)if(source.constScanLine(y)[x*4+3]>(style==3?8:64))crop=crop.united(QRect(x,y,1,1));source=source.copy(crop);}
            QImage original;
            if(style==12){const auto crop=source.copy(QRect(int(source.width()*.065),int(source.height()*.17),int(source.width()*.242),int(source.height()*.63)));original=QImage(pro.canvas,QImage::Format_RGBA8888);original.fill(Qt::transparent);QPainter p(&original);p.setRenderHint(QPainter::SmoothPixmapTransform);p.drawImage(visual::verticalVinylCoverFrame(pro.canvas),crop);}
            else original=source.scaled(pro.canvas,Qt::IgnoreAspectRatio,Qt::SmoothTransformation).convertToFormat(QImage::Format_RGBA8888);
            const int w=original.width(),h=original.height();std::vector<unsigned char> seen(size_t(w)*h);const int seed=geometry.box.center().y()*w+geometry.box.center().x();std::vector<int> hole{seed};seen[seed]=1;int empty=0;
            for(size_t i=0;i<hole.size();++i){const int x=hole[i]%w,y=hole[i]/w;if(original.constScanLine(y)[x*4+3]==0&&composite.pixelColor(x,y).alpha()<254){if(empty<4)std::cerr<<"Gap "<<style<<' '<<x<<','<<y<<" alpha="<<composite.pixelColor(x,y).alpha()<<'\n';++empty;}
                for(const QPoint delta:{QPoint(-1,0),QPoint(1,0),QPoint(0,-1),QPoint(0,1)}){const int nx=x+delta.x(),ny=y+delta.y();if(nx<0||ny<0||nx>=w||ny>=h)continue;const int n=ny*w+nx;if(!seen[n]&&original.constScanLine(ny)[nx*4+3]<64){seen[n]=1;hole.push_back(n);}}}
            if(empty)std::cerr<<"Cover style="<<style<<" source="<<dimensions.width()<<'x'<<dimensions.height()<<" empty="<<empty<<" hole="<<hole.size()<<'\n';
            check(hole.size()>1000&&empty==0,"PRO cover leaves transparency inside original aperture");
            check(composite.pixelColor(geometry.box.center()).green()>250,"PRO cover is not centred in its original aperture");
            if(argc>1&&std::string(argv[1])=="--cover-audit"){QPainter p(&composite);p.drawImage(0,0,original);p.end();composite.save(QString("cover-style-%1.png").arg(style));}
        }

        std::cout<<"PASS "<<checks<<" Track State/shared visual checks; GUI prepare, atomic handoff, two consumers, per-layer cache, inactive and shutdown\n";return 0;
    }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
