// SPDX-License-Identifier: MIT
#include "playing/visual/track-visual.hpp"
#include "playing/visual/vertical-pro-geometry.hpp"
#include "playing/visual/preset-definition.hpp"
#include "playing/visual/performance-profile.hpp"
#include <QGuiApplication>
#include <QThread>
#include <stdexcept>
#include <climits>
#include <QPainter>
#include <QPolygonF>
#include <QPainterPath>
#include <QFontMetricsF>
#include <map>
#include <mutex>
static void initializeProAssets(){Q_INIT_RESOURCE(pro_assets);}
namespace playing::visual {
static QImage proReferenceImage(int style) {
    if(style<1||style>14)throw std::invalid_argument("Unknown PRO style");
    static std::mutex mutex;static std::map<int,QImage> images;std::lock_guard<std::mutex> lock(mutex);
    auto it=images.find(style);if(it!=images.end())return it->second;
    initializeProAssets();const char* names[]={"neon-club-reference-v1.png","vinyl-classic-thin-aligned-v5.png","minimal-reference-v1.png","glassmorph-reference-v1.png","urban-reference-v1.png","waveform-reference-v1.png","hud-reference-v1.png","particles-reference-v1.png","liquid-reference-v1.png","vertical-retro-reference-v3.png","vertical-glass-reference-v1.png","vinyl-classic-thin-aligned-v5.png","vertical-urban-reference-v1.png","vertical-liquid-reference-v2.png"};
    auto image=QImage(QStringLiteral(":/playing/pro-assets/")+QString::fromLatin1(names[style-1])).convertToFormat(QImage::Format_RGBA8888);
    if(image.isNull())throw std::runtime_error("PRO reference unavailable");
    if(style==3||style==4){QRect crop;for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x)if(image.constScanLine(y)[x*4+3]>(style==3?8:64))crop=crop.united(QRect(x,y,1,1));if(!crop.isEmpty())image=image.copy(crop);}
    if(style==12)image=image.copy(QRect(int(image.width()*.065),int(image.height()*.17),int(image.width()*.242),int(image.height()*.63)));
    return images.emplace(style,std::move(image)).first->second;
}
QImage horizontalReferenceImage(int style){if(style<1||style>9)throw std::invalid_argument("Unknown horizontal style");return proReferenceImage(style);}
ProCoverGeometry proCoverGeometry(int style,QSize canvas) {
    if(style<1||style>14)return {};
    struct Reference {QSize size;QRect bounds;QImage mask;QPolygonF quad;};
    static std::mutex mutex;static std::map<int,Reference> cache;
    std::lock_guard<std::mutex> lock(mutex);
    auto it=cache.find(style);
    if(it==cache.end()){
        const auto image=proReferenceImage(style);
        const int w=image.width(),h=image.height();
        const double seedsX[]={.2,.18,.16,.18,.2,.2,.2,.25,.21,.51,.52,.5,.48,.486};
        const double seedsY[]={.5,.5,.5,.5,.5,.5,.5,.5,.5,.41,.38,.5,.35,.42};
        std::vector<unsigned char> seen(size_t(w)*h);std::vector<int> pixels{int(h*seedsY[style-1])*w+int(w*seedsX[style-1])};seen[pixels[0]]=1;QRect bounds;
        for(size_t i=0;i<pixels.size();++i){const int x=pixels[i]%w,y=pixels[i]/w;
            if(x==0||y==0||x==w-1||y==h-1)throw std::runtime_error("PRO cover aperture is open");
            bounds=bounds.united(QRect(x,y,1,1));
            for(const QPoint delta:{QPoint(-1,0),QPoint(1,0),QPoint(0,-1),QPoint(0,1)}){const int nx=x+delta.x(),ny=y+delta.y(),n=ny*w+nx;if(!seen[n]&&image.constScanLine(ny)[nx*4+3]<128){seen[n]=1;pixels.push_back(n);}}
        }
        // Include the antialiased edge beneath the original front rim.
        bounds=bounds.adjusted(-1,-1,1,1);
        QImage mask(bounds.size(),QImage::Format_RGBA8888_Premultiplied);mask.fill(Qt::transparent);
        for(const int index:pixels){const int x=index%w,y=index/w;for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){const int nx=x+dx,ny=y+dy;const int alpha=255-image.constScanLine(ny)[nx*4+3];mask.setPixelColor(nx-bounds.x(),ny-bounds.y(),QColor(255,255,255,alpha));}}
        QPolygonF quad;
        if(style==5){quad={QPointF(179,192),QPointF(637,109),QPointF(713,544),QPointF(257,627)};const auto centre=quad.boundingRect().center();for(auto &p:quad)p=centre+(p-centre)*1.03;}
        else if(style==13){quad={QPointF(171,395),QPointF(654,323),QPointF(723,803),QPointF(237,876)};const auto centre=quad.boundingRect().center();for(auto &p:quad)p=centre+(p-centre)*1.03;}
        else quad={bounds.topLeft(),QPointF(bounds.x()+bounds.width(),bounds.y()),QPointF(bounds.x()+bounds.width(),bounds.y()+bounds.height()),QPointF(bounds.x(),bounds.y()+bounds.height())};
        it=cache.emplace(style,Reference{image.size(),bounds,std::move(mask),quad}).first;
    }
    const auto &ref=it->second;const auto destination=style==12?verticalVinylCoverFrame(canvas):QRectF(QPointF{},QSizeF(canvas));
    const double sx=destination.width()/ref.size.width(),sy=destination.height()/ref.size.height();
    const auto box=QRectF(destination.x()+ref.bounds.x()*sx,destination.y()+ref.bounds.y()*sy,ref.bounds.width()*sx,ref.bounds.height()*sy).toAlignedRect();
    QPolygonF quad;for(const auto p:ref.quad)quad<<QPointF(destination.x()+p.x()*sx-box.x(),destination.y()+p.y()*sy-box.y());
    // Preserve the same subpixel mapping used to scale the frame itself.
    QImage mask(box.size(),QImage::Format_RGBA8888_Premultiplied);mask.fill(Qt::transparent);
    {QPainter p(&mask);p.setRenderHint(QPainter::SmoothPixmapTransform);p.drawImage(QRectF(destination.x()+ref.bounds.x()*sx-box.x(),destination.y()+ref.bounds.y()*sy-box.y(),ref.bounds.width()*sx,ref.bounds.height()*sy),ref.mask);}
    for(int y=0;y<mask.height();++y)for(int x=0;x<mask.width();++x)if(mask.constScanLine(y)[x*4+3]>=250)mask.setPixelColor(x,y,Qt::white);
    return {box,std::move(quad),std::move(mask)};
}
ProCoverGeometry horizontalCoverGeometry(int style,QSize canvas){return style>=1&&style<=9?proCoverGeometry(style,canvas):ProCoverGeometry{};}
namespace {
// Linear-time premultiplied blur. Prepared once per size, never during draw/hue.
QImage diffuseLight(QImage image,int radius) {
    for(int pass=0;pass<4;++pass){
        const bool horizontal=pass%2==0;QImage output(image.size(),image.format());output.fill(Qt::transparent);
        const int lines=horizontal?image.height():image.width(),length=horizontal?image.width():image.height(),divisor=radius*2+1;
        for(int line=0;line<lines;++line){
            std::array<int,4> sum{};
            const auto pixel=[&](int index){return horizontal?image.constScanLine(line)+index*4:image.constScanLine(index)+line*4;};
            for(int index=0;index<=radius&&index<length;++index)for(int c=0;c<4;++c)sum[c]+=pixel(index)[c];
            for(int index=0;index<length;++index){
                auto *destination=horizontal?output.scanLine(line)+index*4:output.scanLine(index)+line*4;
                for(int c=0;c<4;++c)destination[c]=static_cast<uchar>((sum[c]+divisor/2)/divisor);
                if(index-radius>=0)for(int c=0;c<4;++c)sum[c]-=pixel(index-radius)[c];
                if(index+radius+1<length)for(int c=0;c<4;++c)sum[c]+=pixel(index+radius+1)[c];
            }
        }
        image=std::move(output);
    }
    return image;
}
void owner() {
    if(!qobject_cast<QGuiApplication *>(QCoreApplication::instance())||QThread::currentThread()!=qApp->thread())
        throw std::logic_error("Track visual host requires GUI thread");
}
void validate(const TrackLayout &layout) {
    const QRect bounds(QPoint{},layout.canvas);
    if(layout.canvas.width()<1||layout.canvas.height()<1||layout.canvas.width()>2048||layout.canvas.height()>2048
        ||!layout.title.box.isValid()||!layout.artist.box.isValid()||!bounds.contains(layout.title.box)||!bounds.contains(layout.artist.box))
        throw std::invalid_argument("Track layout outside canvas");
}
}
void validateTrackLayout(const TrackLayout &layout) {
    performanceCadence(layout.performanceProfile);
    for(const auto &effect:layout.textEffects)effect.validate();
    animation::program(layout.animationProfile);
    if(layout.animationSpeed<25||layout.animationSpeed>300)throw std::invalid_argument("Animation speed outside limits");
    if(layout.proPreset<0||layout.proPreset>14||layout.proHue<0||layout.proHue>360||layout.proLightIntensity<0||layout.proLightIntensity>100||layout.proPanelOpacity<0||layout.proPanelOpacity>100||(layout.proPreset&&layout.normalPreset))throw std::invalid_argument("Invalid PRO pilot controls");
    if(layout.normalDuration<200||layout.normalDuration>2000||
        (layout.normalTransition!=normal::Transition::Fade&&layout.normalTransition!=normal::Transition::Slide&&layout.normalTransition!=normal::Transition::Zoom))throw std::invalid_argument("Track transition outside limits");
    if(layout.artworkEnabled&&(!layout.artworkBox.isValid()||layout.artworkBox.x()<0||layout.artworkBox.y()<0||layout.artworkBox.right()>=layout.canvas.width()||layout.artworkBox.bottom()>=2048))throw std::invalid_argument("Artwork outside canvas limits");
    if(layout.normalPreset<0)throw std::invalid_argument("Unverified NORMAL preset");
    if(layout.normalPreset)normal::presetFromSetting(layout.normalPreset);
    if(layout.normalPreset) {
        for(const auto &override:layout.normalText)override.effects.validate();
        normal::customizedDefinition(normal::presetDefinition(normal::presetFromSetting(layout.normalPreset)),layout.normalText);
        if(layout.canvas.width()<400||layout.canvas.width()>1600)throw std::invalid_argument("NORMAL width outside original range");
        if(layout.normalDuration<200||layout.normalDuration>2000||layout.normalGap<0||layout.normalGap>80
            ||!std::isfinite(layout.normalSpeed)||layout.normalSpeed<10||layout.normalSpeed>150
            ||!std::isfinite(layout.normalPause)||layout.normalPause<0||layout.normalPause>10
            ||(layout.normalTransition!=normal::Transition::Fade&&layout.normalTransition!=normal::Transition::Slide&&layout.normalTransition!=normal::Transition::Zoom))
            throw std::invalid_argument("NORMAL motion outside original range");
        return;
    }
    validate(layout);
    oto::visual::TextLayoutEngine::validate(layout.title);
    oto::visual::TextLayoutEngine::validate(layout.artist);
}
TrackFramePtr TrackVisualHost::prepareIdle(const TrackLayout &layout) {
    owner();validateTrackLayout(layout);
    if(!layout.normalPreset){auto frame=std::make_shared<TrackFrame>();frame->canvas=layout.canvas;frame->normalSample.visible=true;if(layout.proPreset){frame->proArt=prepareNeonClub(layout);frame->lightGain=layout.proLightIntensity/100.0;frame->proHue=layout.proHue;frame->proStyle=layout.proPreset;frame->panelGain=layout.proPanelOpacity/100.0;}return frame;}
    auto snapshot=std::make_shared<core::Snapshot>();snapshot->running=true;
    snapshot->playback=core::Playback::Playing;snapshot->track=std::make_shared<const core::TrackState>(core::TrackState{" ",""});
    auto frame=std::make_shared<TrackFrame>(*prepare(snapshot,layout));
    frame->snapshot.reset();frame->title.reset();frame->artist.reset();frame->artwork.reset();frame->scrollingText={};frame->scrollingDistance={};
    if(frame->normalFrame){auto normal=std::make_shared<normal::PreparedNormal>(*frame->normalFrame);normal->text={};frame->normalFrame=normal;}
    frame->normalSample.visible=true;frame->wholeTransition=false;return frame;
}
TrackFramePtr TrackVisualHost::prepare(core::SnapshotPtr snapshot,const TrackLayout &layout,bool active) {
    owner();
    if(!active||!snapshot||!snapshot->running||snapshot->playback!=core::Playback::Playing||!snapshot->track) return {};
    if(!core::validMetadata(*snapshot->track)) throw std::invalid_argument("Invalid track snapshot");
    validateTrackLayout(layout);
    if(layout.normalPreset) {
        auto result=std::make_shared<TrackFrame>();result->snapshot=std::move(snapshot);
        result->normalFrame=prepareNormal(normal::presetFromSetting(layout.normalPreset),result->snapshot->track,layout.canvas.width(),layout.normalGap,layout.normalText);
        result->normalSpeed=layout.normalSpeed;result->normalPause=layout.normalPause;
        result->normalSample.track=result->snapshot->track;result->normalSample.visible=true;
        result->canvas=result->normalFrame->layout.canvas;
        return result;
    }
    const auto layer=[this,&layout](oto::visual::TextNode node,const std::string &text,const TextEffects &effect) -> oto::visual::FramePtr {
        if(text.empty()) return {};
        const auto size=node.box.size(); node.box.moveTopLeft(QPoint{});
        const int effectPad=effect.padding();if(effectPad&&node.box.width()>effectPad*2&&node.box.height()>effectPad*2)node.box.adjust(effectPad,effectPad,-effectPad,-effectPad);
        node.text=QString::fromUtf8(text.data(),static_cast<qsizetype>(text.size()));
        if(layout.proPreset){
            node.vertical=oto::visual::Vertical::InkCenter;
            QFont font(node.family);font.setBold(node.bold);font.setItalic(node.italic);
            do{font.setPixelSize(node.pixelSize);if(QFontMetricsF(font).height()<=node.box.height()-2||node.pixelSize<=8)break;--node.pixelSize;}while(true);
            node.minimumPixelSize=std::min(node.minimumPixelSize,node.pixelSize);
            const int pad=std::min(size.width()/4,std::max(2,int(std::ceil(node.pixelSize*(node.italic?.2:.05)))));
            if(size.height()>2&&size.width()>2*pad)node.box.adjust(pad,1,-pad,-1);
        }
        if(layout.proPreset==12&&node.horizontal==oto::visual::Horizontal::Center){
            const auto shaped=oto::visual::TextLayoutEngine{}.prepare(node);QRectF ink;
            for(const auto &run:shaped.layout->glyphRuns()){const auto glyphs=run.glyphIndexes();const auto positions=run.positions();for(qsizetype i=0;i<glyphs.size();++i){const auto bounds=run.rawFont().pathForGlyph(glyphs[i]).boundingRect();if(!bounds.isEmpty())ink=ink.united(bounds.translated(positions[i]));}}
            if(!ink.isEmpty()&&ink.width()<=node.box.width())node.box.translate(qRound(node.box.width()/2.-ink.center().x()),0);
        }
        const auto prepared=engine_.prepare({size,{node}});if(!effectPad&&!effect.underline&&!effect.strikeout)return prepared;auto pixels=prepared->image.copy();if(effect.underline||effect.strikeout){const auto shape=oto::visual::TextLayoutEngine{}.prepare(node);QPainter painter(&pixels);drawTextDecorations(painter,*shape.layout,effect,node.color,QPointF(node.box.x(),node.box.y()+shape.verticalOffset));}return engine_.prepareRaster(applyTextEffects(std::move(pixels),effect));
    };
    auto result=std::make_shared<TrackFrame>(); result->snapshot=std::move(snapshot); result->canvas=layout.canvas;
    if(layout.proPreset){result->proArt=prepareNeonClub(layout);result->lightGain=layout.proLightIntensity/100.0;result->proHue=layout.proHue;result->proStyle=layout.proPreset;result->panelGain=layout.proPanelOpacity/100.0;}
    result->titlePosition=layout.title.box.topLeft(); result->artistPosition=layout.artist.box.topLeft();
    result->scrollingViewport={layout.title.box,layout.artist.box};
    result->title=layer(layout.title,result->snapshot->track->title,layout.textEffects[0]);
    result->artist=layer(layout.artist,result->snapshot->track->artist,layout.textEffects[1]);
    result->normalSpeed=layout.normalSpeed;result->normalPause=layout.normalPause;
    for(std::size_t i=0;i<2;++i)if(i?layout.artistPingPong:layout.titlePingPong){
        auto node=i?layout.artist:layout.title;node.box.moveTopLeft(QPoint{});node.overflow=oto::visual::Overflow::Clip;node.horizontal=oto::visual::Horizontal::Left;
        const auto &effect=layout.textEffects[i];const int effectPad=effect.padding();
        if(effectPad&&node.box.height()>effectPad*2)node.box.adjust(0,effectPad,0,-effectPad);
        int glyphPad=0;
        if(layout.proPreset){node.vertical=oto::visual::Vertical::InkCenter;QFont font(node.family);font.setBold(node.bold);font.setItalic(node.italic);
            do{font.setPixelSize(node.pixelSize);if(QFontMetricsF(font).height()<=node.box.height()-2||node.pixelSize<=8)break;--node.pixelSize;}while(true);
            node.minimumPixelSize=std::min(node.minimumPixelSize,node.pixelSize);glyphPad=std::max(2,int(std::ceil(node.pixelSize*(node.italic?.2:.05))));}
        node.text=QString::fromUtf8((i?result->snapshot->track->artist:result->snapshot->track->title).c_str());
        const auto shaped=oto::visual::TextLayoutEngine{}.prepare(node);
        const int width=int(std::ceil(shaped.metrics.width))+2+glyphPad*2+effectPad*2;
        if(width>node.box.width()){
            // Bound pathological metadata without allocating a single giant texture.
            if(width>65536||int64_t(width)*node.box.height()>8388608)throw std::invalid_argument("Scrolling text exceeds raster budget");
            result->scrollingViewport[i]=i?layout.artist.box:layout.title.box;result->scrollingDistance[i]=width-node.box.width();
            const int gutter=1+effectPad,span=2048-2*gutter;
            for(int x=0;x<width;x+=span){
                const int content=std::min(span,width-x),left=std::max(0,x-gutter),right=std::min(width,x+content+gutter);
                QImage pixels(QSize(right-left,(i?layout.artist:layout.title).box.height()),QImage::Format_RGBA8888_Premultiplied);pixels.fill(Qt::transparent);
                {QPainter painter(&pixels);painter.setPen(node.color);const QPointF position(-left+glyphPad+effectPad,shaped.verticalOffset+effectPad);shaped.layout->draw(&painter,position);drawTextDecorations(painter,*shaped.layout,effect,node.color,position);}
                result->scrollingText[i].push_back({left,x,content,engine_.prepareRaster(applyTextEffects(std::move(pixels),effect))});
            }
            if(i)result->artist.reset();else result->title.reset();
        }
    }
    if(layout.proPreset){
        const auto ink=[](const oto::visual::FramePtr &pixels,const std::vector<normal::TextTile> &tiles){QPair<int,int> bounds{INT_MAX,-1};const auto scan=[&](const oto::visual::FramePtr &frame){if(!frame)return;for(int y=0;y<frame->image.height();++y){const auto *row=frame->image.constScanLine(y);for(int x=0;x<frame->image.width();++x)if(row[x*4+3]>0){bounds.first=std::min(bounds.first,y);bounds.second=std::max(bounds.second,y);break;}}};scan(pixels);for(const auto &tile:tiles)scan(tile.pixels);return bounds;};
        const auto titleInk=ink(result->title,result->scrollingText[0]),artistInk=ink(result->artist,result->scrollingText[1]);
        if(titleInk.second>=0&&artistInk.second>=0){const int gap=layout.artist.box.top()-layout.title.box.bottom()-1;const int artistTop=result->titlePosition.y()+titleInk.second+1+gap-artistInk.first;result->artistPosition.setY(artistTop);result->scrollingViewport[1].moveTop(artistTop);}
    }
    return result;
}
ProArtPtr TrackVisualHost::prepareNeonClub(const TrackLayout &layout) {
    owner();const auto cover=(layout.artworkEnabled||layout.proPreset==2)?layout.artworkBox:QRect{};const int hue=layout.proPreset+(layout.artworkEnabled?0:16); // Style cache key. Tone is a GPU parameter; never rebuild pixels while dragging.
    for(auto it=proCache_.begin();it!=proCache_.end();) {
        if(auto art=it->art.lock()){if(it->canvas==layout.canvas&&it->cover==cover&&it->hue==hue){proCache_.splice(proCache_.begin(),proCache_,it);return art;}++it;}
        else it=proCache_.erase(it);
    }
    initializeProAssets();
    const auto horizontal=horizontalReferenceImage(layout.proPreset<=9?layout.proPreset:1);
    const auto vinyl=horizontalReferenceImage(2);
    const QImage *selected=&horizontal;
    if(layout.proPreset==10){static const QImage retro=proReferenceImage(10);selected=&retro;}
    if(layout.proPreset==11){static const QImage verticalGlass=proReferenceImage(11);selected=&verticalGlass;}
    if(layout.proPreset==13){static const QImage verticalUrban=proReferenceImage(13);selected=&verticalUrban;}
    if(layout.proPreset==14){static const QImage verticalLiquid=proReferenceImage(14);selected=&verticalLiquid;}
    QImage verticalVinyl,verticalVinylPanel;
    if(layout.proPreset==12){
        verticalVinyl=QImage(layout.canvas,QImage::Format_RGBA8888_Premultiplied);verticalVinyl.fill(Qt::transparent);
        const int w=layout.canvas.width(),h=layout.canvas.height(),rw=vinyl.width(),rh=vinyl.height();
        QPainter painter(&verticalVinyl);painter.setRenderHint(QPainter::SmoothPixmapTransform);
        const QRect bodyCrop(int(rw*.435),int(rh*.17),int(rw*.545),int(rh*.63));
        const auto body=vinyl.copy(bodyCrop).transformed(QTransform().rotate(90));
        painter.drawImage(QRectF(w*.09,h*.26,w*.82,h*.67),body);
        // Separate the interior before applying the exact same crop/rotation.
        // Gold bevels and luminous markings stay in the immutable frame plane.
        QImage sourcePanel(vinyl.size(),QImage::Format_RGBA8888_Premultiplied);sourcePanel.fill(Qt::transparent);
        const QPolygonF inner{QPointF(.44,.29),QPointF(.88,.29),QPointF(.94,.385),QPointF(.94,.642),QPointF(.90,.725),QPointF(.44,.725)};
        for(int y=bodyCrop.top();y<=bodyCrop.bottom();++y)for(int x=bodyCrop.left();x<=bodyCrop.right();++x){
            const auto color=vinyl.pixelColor(x,y);
            if(inner.containsPoint(QPointF((x+.5)/rw,(y+.5)/rh),Qt::OddEvenFill)&&color.valueF()<.4)sourcePanel.setPixelColor(x,y,color);
        }
        verticalVinylPanel=QImage(layout.canvas,QImage::Format_RGBA8888_Premultiplied);verticalVinylPanel.fill(Qt::transparent);
        {QPainter p(&verticalVinylPanel);p.setRenderHint(QPainter::SmoothPixmapTransform);
            p.drawImage(QRectF(w*.09,h*.26,w*.82,h*.67),sourcePanel.copy(bodyCrop).transformed(QTransform().rotate(90)));}
        const int rx=int(rw*.137),ry=int(rh*.37),cx=int(rw*.313),cy=int(rh*.49);
        QImage ring(QSize(rx*2,ry*2),QImage::Format_RGBA8888_Premultiplied);ring.fill(Qt::transparent);
        for(int y=0;y<ry*2;++y)for(int x=rx;x<rx*2;++x){
            const double dx=(x-rx)/double(rx),dy=(y-ry)/double(ry),radius=std::sqrt(dx*dx+dy*dy);
            const int sx=cx-rx+x,sy=cy-ry+y;
            if(radius>.96&&radius<1.03&&sx>=0&&sy>=0&&sx<rw&&sy<rh)ring.setPixelColor(x,y,vinyl.pixelColor(sx,sy));
        }
        painter.drawImage(QRectF(w*.12,h*.05,w*.76,w*.76),ring.transformed(QTransform().rotate(-90)));
        painter.drawImage(verticalVinylCoverFrame(layout.canvas),vinyl.copy(QRect(int(rw*.065),int(rh*.17),int(rw*.242),int(rh*.63))));
        painter.end();selected=&verticalVinyl;
    }
    const auto &reference=*selected;
    if(reference.isNull())throw std::runtime_error("Neon Club reference asset unavailable");
    const auto scaled=reference.scaled(layout.canvas,Qt::IgnoreAspectRatio,Qt::SmoothTransformation).convertToFormat(QImage::Format_RGBA8888);
    QImage background(layout.canvas,QImage::Format_RGBA8888_Premultiplied);background.fill(Qt::transparent);
    QImage light(layout.canvas,QImage::Format_RGBA8888_Premultiplied);light.fill(Qt::transparent);
    background=scaled.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
    QImage panel(layout.canvas,QImage::Format_RGBA8888_Premultiplied);panel.fill(Qt::transparent);
    if(layout.proPreset==6||layout.proPreset==7||layout.proPreset==14){const int w=scaled.width(),h=scaled.height();std::vector<unsigned char> seen(size_t(w)*h);std::vector<int> pending{int(h*(layout.proPreset==14?.73:.5))*w+int(w*(layout.proPreset==14?.5:.6))};seen[pending[0]]=1;bool open=false;for(size_t i=0;i<pending.size();++i){const int index=pending[i],x=index%w,y=index/w;if(x==0||y==0||x==w-1||y==h-1)open=true;for(const QPoint delta:{QPoint(-1,0),QPoint(1,0),QPoint(0,-1),QPoint(0,1)}){const int nx=x+delta.x(),ny=y+delta.y();if(nx<0||ny<0||nx>=w||ny>=h)continue;const int n=ny*w+nx;if(!seen[n]&&scaled.constScanLine(ny)[nx*4+3]<64){seen[n]=1;pending.push_back(n);}}}if(open&&layout.proPreset==14)throw std::runtime_error("Vertical Liquid text aperture must be closed");if(!open)for(const int index:pending)panel.setPixelColor(index%w,index/w,QColor(5,8,18,255));}
    // Exact disjoint pixel split: opacity affects only the interior dark material.
    // Trace the inside lip of Neon Club in its original 2030x775 reference.
    // The background reaches the cover rim, upper rail and lower stepped edge.
    const QPolygonF interior{QPointF(711./2030,234./775),QPointF(790./2030,234./775),QPointF(806./2030,228./775),QPointF(1763./2030,228./775),QPointF(1854./2030,319./775),QPointF(1854./2030,499./775),QPointF(1817./2030,540./775),QPointF(1817./2030,551./775),QPointF(1782./2030,562./775),QPointF(1534./2030,562./775),QPointF(1492./2030,605./775),QPointF(762./2030,605./775),QPointF(741./2030,624./775),QPointF(711./2030,626./775)};
    const QPolygonF minimalInterior{QPointF(.282,.145),QPointF(.343,.145),QPointF(.357,.16),QPointF(.925,.16),QPointF(.955,.265),QPointF(.955,.77),QPointF(.918,.87),QPointF(.858,.87),QPointF(.842,.805),QPointF(.392,.805),QPointF(.376,.87),QPointF(.282,.87)};
    const QPolygonF urbanInterior{QPointF(.34,.32),QPointF(.90,.20),QPointF(.88,.66),QPointF(.36,.81)};
    QPainterPath glassInterior;glassInterior.addRoundedRect(QRectF(.30,.13,.65,.75),.035,.10);
    // Liquid's dark chrome is part of the frame, not part of the title background.
    // Follow the inner lip independently of colour, including its blue gradient.
    QPainterPath liquidInterior;
    const QPolygonF liquidLip{
        QPointF(340./960,160./320),QPointF(360./960,121./320),QPointF(380./960,105./320),
        QPointF(400./960,94./320),QPointF(420./960,87./320),QPointF(440./960,83./320),
        QPointF(460./960,82./320),QPointF(480./960,87./320),QPointF(500./960,87./320),
        QPointF(520./960,93./320),QPointF(540./960,99./320),QPointF(560./960,105./320),
        QPointF(580./960,110./320),QPointF(600./960,112./320),QPointF(620./960,111./320),
        QPointF(640./960,109./320),QPointF(660./960,105./320),QPointF(680./960,99./320),
        QPointF(700./960,95./320),QPointF(720./960,92./320),QPointF(740./960,92./320),
        QPointF(760./960,94./320),QPointF(780./960,100./320),QPointF(800./960,109./320),
        QPointF(820./960,124./320),QPointF(840./960,149./320),QPointF(850./960,180./320),
        QPointF(840./960,231./320),QPointF(820./960,209./320),QPointF(800./960,202./320),
        QPointF(780./960,203./320),QPointF(760./960,214./320),QPointF(740./960,247./320),
        QPointF(720./960,246./320),QPointF(700./960,243./320),QPointF(680./960,242./320),
        QPointF(660./960,243./320),QPointF(640./960,247./320),QPointF(620./960,252./320),
        QPointF(600./960,259./320),QPointF(580./960,264./320),QPointF(560./960,269./320),
        QPointF(540./960,271./320),QPointF(520./960,272./320),QPointF(500./960,272./320),
        QPointF(480./960,269./320),QPointF(460./960,265./320),QPointF(440./960,260./320),
        QPointF(420./960,253./320),QPointF(400./960,246./320),QPointF(380./960,240./320),
        QPointF(360./960,236./320),QPointF(340./960,171./320)};
    liquidInterior.addPolygon(liquidLip);liquidInterior.closeSubpath();
    // Background contour excludes the lower palm groups and the neon lip.
    QPainterPath verticalGlassInterior;verticalGlassInterior.addRoundedRect(QRectF(.145,.545,.70,.335),.04,.03);QPainterPath glassBubble;glassBubble.addEllipse(QRectF(.65,.81,.22,.12));glassBubble.addEllipse(QRectF(.78,.69,.16,.12));verticalGlassInterior=verticalGlassInterior.subtracted(glassBubble);
    const QPolygonF retroBackground{QPointF(.19,.578),QPointF(.79,.578),QPointF(.79,.71),QPointF(.69,.77),QPointF(.70,.90),QPointF(.64,.94),QPointF(.36,.94),QPointF(.35,.78),QPointF(.29,.73),QPointF(.19,.65)};
    for(int y=0;y<background.height();++y)for(int x=0;x<background.width();++x)
        if(layout.proPreset==14?false:layout.proPreset==13?(scaled.pixelColor(x,y).valueF()<.38&&(scaled.pixelColor(x,y).saturationF()<.5||scaled.pixelColor(x,y).valueF()<.12)):layout.proPreset==12?(verticalVinylPanel.constScanLine(y)[x*4+3]>0&&scaled.pixelColor(x,y).valueF()<.4):layout.proPreset==11?(verticalGlassInterior.contains(QPointF((x+.5)/background.width(),(y+.5)/background.height()))):layout.proPreset==10?(retroBackground.containsPoint(QPointF((x+.5)/background.width(),(y+.5)/background.height()),Qt::OddEvenFill)&&scaled.pixelColor(x,y).valueF()<.30&&scaled.pixelColor(x,y).blue()>18&&scaled.pixelColor(x,y).blue()>scaled.pixelColor(x,y).red()*1.05&&scaled.pixelColor(x,y).blue()>scaled.pixelColor(x,y).green()*1.2):layout.proPreset==9?liquidInterior.contains(QPointF((x+.5)/background.width(),(y+.5)/background.height())):(layout.proPreset==6||layout.proPreset==7)?false:(layout.proPreset==5||layout.proPreset==8)?(scaled.pixelColor(x,y).valueF()<.38&&(scaled.pixelColor(x,y).saturationF()<.5||scaled.pixelColor(x,y).valueF()<.12)):layout.proPreset==4?glassInterior.contains(QPointF((x+.5)/background.width(),(y+.5)/background.height())):layout.proPreset==3?minimalInterior.containsPoint(QPointF((x+.5)/background.width(),(y+.5)/background.height()),Qt::OddEvenFill):layout.proPreset==2?QRectF(.415,.225,.535,.55).contains(QPointF((x+.5)/background.width(),(y+.5)/background.height())):interior.containsPoint(QPointF((x+.5)/background.width(),(y+.5)/background.height()),Qt::OddEvenFill)){
            const auto color=scaled.pixelColor(x,y);if(layout.proPreset!=1&&layout.proPreset!=4&&layout.proPreset!=9&&layout.proPreset!=11&&layout.proPreset!=12&&color.valueF()>.4)continue;
            const auto offset=x*4;std::copy_n(background.constScanLine(y)+offset,4,panel.scanLine(y)+offset);
            std::fill_n(background.scanLine(y)+offset,4,0);
        }
    // Extract continuous luminous bands from the actual frame, not point emitters.
    // Quarter-resolution diffusion is bounded and cached; 0% keeps the whole frame.
    const QSize smallSize(std::max(1,scaled.width()/4),std::max(1,scaled.height()/4));
    const auto bands=scaled.scaled(smallSize,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
    QImage emission(smallSize,QImage::Format_RGBA8888_Premultiplied);emission.fill(Qt::transparent);
    for(int y=0;y<bands.height();++y)for(int x=0;x<bands.width();++x){
        auto color=bands.pixelColor(x,y);
        const double strength=layout.proPreset==5?(color.saturationF()>.5&&!urbanInterior.containsPoint(QPointF((x+.5)/bands.width(),(y+.5)/bands.height()),Qt::OddEvenFill)?std::clamp((color.valueF()-.6)/.4,0.0,1.0):0.0):layout.proPreset==4?(glassInterior.contains(QPointF((x+.5)/bands.width(),(y+.5)/bands.height()))?0.0:std::clamp((color.valueF()-.80)/.20,0.0,1.0)):std::clamp((color.valueF()-.45)/.55,0.0,1.0);
        color.setAlphaF(color.alphaF()*strength);
        emission.setPixelColor(x,y,color);
    }
    light=diffuseLight(std::move(emission),layout.proPreset==5?1:(layout.proPreset==4||layout.proPreset==6)?2:8).scaled(layout.canvas,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
    if(layout.proPreset==6||layout.proPreset==7||layout.proPreset==8||layout.proPreset==9||layout.proPreset==10||layout.proPreset==11||layout.proPreset==12||layout.proPreset==14){light.fill(Qt::transparent);for(int y=0;y<scaled.height();++y)for(int x=0;x<scaled.width();++x){auto color=scaled.pixelColor(x,y);if(color.valueF()<.5)continue;color.setRedF(.45*color.redF()+.55);color.setGreenF(.45*color.greenF()+.55);color.setBlueF(.45*color.blueF()+.55);light.setPixelColor(x,y,color);}}
    if(layout.proPreset==4)for(int y=0;y<light.height();++y)for(int x=0;x<light.width();++x){const double nx=(x+.5)/light.width(),ny=(y+.5)/light.height();if(nx>.055&&nx<.945&&ny>.14&&ny<.86)std::fill_n(light.scanLine(y)+x*4,4,0);}
    if(layout.proPreset==6||layout.proPreset==7||layout.proPreset==9||layout.proPreset==10||layout.proPreset==11||layout.proPreset==12||layout.proPreset==14)for(int y=0;y<light.height();++y)for(int x=0;x<light.width();++x)if(panel.constScanLine(y)[x*4+3]>0)std::fill_n(light.scanLine(y)+x*4,4,0);
    if(layout.proPreset==7){light.fill(Qt::transparent);for(int y=0;y<scaled.height();++y)for(int x=0;x<scaled.width();++x){const double nx=(x+.5)/scaled.width(),ny=(y+.5)/scaled.height();auto color=scaled.pixelColor(x,y);if(nx<.315||nx>.56||!((ny>.10&&ny<.15)||(ny>.84&&ny<.89))||color.saturationF()<.5||color.valueF()<.4)continue;light.setPixelColor(x,y,color);color.setRedF(color.redF()*.18);color.setGreenF(color.greenF()*.18);color.setBlueF(color.blueF()*.18);background.setPixelColor(x,y,color);}}
    // Stronger broad halo, preserving premultiplication and the unchanged frame at 0%.
    if(layout.proPreset!=6&&layout.proPreset!=7&&layout.proPreset!=8&&layout.proPreset!=9&&layout.proPreset!=10&&layout.proPreset!=11&&layout.proPreset!=12&&layout.proPreset!=14)for(int y=0;y<light.height();++y)for(int x=0;x<light.width()*4;++x){auto &channel=light.scanLine(y)[x];channel=uchar(std::min(255,int(channel)*2));}
    if(layout.artworkEnabled){const auto aperture=proCoverGeometry(layout.proPreset,layout.canvas);for(QImage *plane:{&background,&light,&panel}){QPainter painter(plane);painter.setCompositionMode(QPainter::CompositionMode_DestinationOut);painter.drawImage(aperture.box.topLeft(),aperture.mask);}}
    std::vector<ProPrism> prisms;
    if(layout.proPreset==8){const QPolygonF shapes[]={QPolygonF{QPointF(.55,.235),QPointF(.605,.0),QPointF(.69,.055)},QPolygonF{QPointF(.41,.70),QPointF(.54,.80),QPointF(.46,1.0)},QPolygonF{QPointF(.775,.015),QPointF(.817,.015),QPointF(.828,.15),QPointF(.769,.15)}};for(const auto &shape:shapes){QImage crystal(layout.canvas,QImage::Format_RGBA8888_Premultiplied),shine(layout.canvas,QImage::Format_RGBA8888_Premultiplied);crystal.fill(Qt::transparent);shine.fill(Qt::transparent);QRect bounds;for(int y=0;y<scaled.height();++y)for(int x=0;x<scaled.width();++x){auto color=scaled.pixelColor(x,y);if(!shape.containsPoint(QPointF((x+.5)/scaled.width(),(y+.5)/scaled.height()),Qt::OddEvenFill)||color.alpha()==0||color.valueF()<.14)continue;crystal.setPixelColor(x,y,color);auto glint=color;glint.setRedF(glint.redF()*.45+.55);glint.setGreenF(glint.greenF()*.45+.55);glint.setBlueF(glint.blueF()*.45+.55);shine.setPixelColor(x,y,glint);bounds=bounds.united(QRect(x,y,1,1));std::fill_n(background.scanLine(y)+x*4,4,0);std::fill_n(light.scanLine(y)+x*4,4,0);std::fill_n(panel.scanLine(y)+x*4,4,0);}if(!bounds.isEmpty())prisms.push_back({engine_.prepareRaster(crystal.copy(bounds)),engine_.prepareRaster(shine.copy(bounds)),bounds.topLeft()});}}
    auto art=std::make_shared<ProArt>();art->prisms=std::move(prisms);art->backdrop=engine_.prepareRaster(std::move(background));art->light=engine_.prepareRaster(std::move(light));art->panel=engine_.prepareRaster(std::move(panel));
    if(layout.proPreset==5||layout.proPreset==13){QImage accent(layout.canvas,QImage::Format_RGBA8888_Premultiplied);accent.fill(Qt::transparent);for(int y=0;y<scaled.height();++y)for(int x=0;x<scaled.width();++x){auto color=scaled.pixelColor(x,y);if(color.saturationF()<.5||color.valueF()<.4||(layout.proPreset==13&&art->backdrop->image.constScanLine(y)[x*4+3]==0))continue;color.setRedF(color.redF()*.35+.65);color.setGreenF(color.greenF()*.35+.65);color.setBlueF(color.blueF()*.35+.65);accent.setPixelColor(x,y,color);}art->spray=engine_.prepareRaster(std::move(accent));}
    if(layout.proPreset==2||layout.proPreset==12){
        // Match the selected frame aperture; cover spacing does not move its circle.
        const int size=std::max(8,int(layout.proPreset==12?layout.canvas.width()*.76:layout.canvas.height()*.74));
        static const QImage recordReference=QImage(":/playing/pro-assets/vinyl-record-realistic-v1.png").convertToFormat(QImage::Format_RGBA8888_Premultiplied);
        if(recordReference.isNull())throw std::runtime_error("Vinyl record asset missing");
        auto record=recordReference.scaled(QSize(size,size),Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
        QImage reflection(record.size(),QImage::Format_RGBA8888_Premultiplied);reflection.fill(Qt::transparent);
        for(int y=0;y<record.height();++y)for(int x=0;x<record.width();++x){const auto c=record.pixelColor(x,y);if(c.saturationF()>.20||c.valueF()<.35)continue;QColor highlight(Qt::white);highlight.setAlphaF(c.alphaF()*(c.valueF()-.35)/.65);reflection.setPixelColor(x,y,highlight);}
        art->recordReflection=engine_.prepareRaster(std::move(reflection));
        art->recordPosition=layout.proPreset==12?QPoint(int(layout.canvas.width()*.12),int(layout.canvas.height()*.05)):QPoint(int(layout.canvas.width()*.313)-size/2,int(layout.canvas.height()*.49)-size/2);art->record=engine_.prepareRaster(std::move(record));
        QImage ink(QSize(size,size),QImage::Format_RGBA8888_Premultiplied);ink.fill(Qt::transparent);
        {QPainter p(&ink);p.setRenderHint(QPainter::Antialiasing);
         // Contrasting asymmetric paper print: visible motion, never a moving light.
         p.setPen(Qt::NoPen);p.setBrush(QColor(245,233,203,245));p.drawPie(QRectF(size*.37,size*.37,size*.26,size*.26),25*16,95*16);
         p.setCompositionMode(QPainter::CompositionMode_Clear);p.drawEllipse(QPointF(size*.5,size*.5),size*.065,size*.065);p.setCompositionMode(QPainter::CompositionMode_SourceOver);
         p.setPen(QColor(25,10,8,240));QFont font("Segoe UI");font.setPixelSize(std::max(8,int(size*.05)));font.setBold(true);p.setFont(font);p.drawText(QPointF(size*.515,size*.485),QStringLiteral("33⅓"));
         p.setPen(QPen(QColor(245,233,203,230),std::max(2.0,size*.013)));p.drawArc(QRectF(size*.385,size*.385,size*.23,size*.23),210*16,35*16);}
        art->recordInk=engine_.prepareRaster(std::move(ink));
    }
    proCache_.push_front({layout.canvas,cover,hue,art});if(proCache_.size()>8)proCache_.pop_back();return art;
}
normal::PreparedNormalPtr TrackVisualHost::prepareNormal(normal::NativePreset preset,normal::TrackPtr track,int width,int gap,const normal::TextOverrides &overrides) {
    owner();
    if(!track||!core::validMetadata(*track))throw std::invalid_argument("Invalid NORMAL track");
    for(auto it=normalCache_.begin();it!=normalCache_.end();) {
        const auto cached=it->frame.lock();
        if(!cached){it=normalCache_.erase(it);continue;}
        if(it->preset==preset&&it->width==width&&it->gap==gap&&it->overrides==overrides&&cached->track->title==track->title&&cached->track->artist==track->artist) {
            normal::PreparedNormalPtr result=cached;
            if(cached->track!=track){auto copy=std::make_shared<normal::PreparedNormal>(*cached);copy->track=std::move(track);result=copy;it->frame=result;}
            normalCache_.splice(normalCache_.begin(),normalCache_,it);return result;
        }
        ++it;
    }
    const auto result=normal::prepareNativeNormal(engine_,preset,std::move(track),width,QStringLiteral("NOW PLAYING"),gap,overrides);
    normalCache_.push_front({preset,width,gap,result,overrides});if(normalCache_.size()>32)normalCache_.pop_back();
    return result;
}
oto::visual::FramePtr TrackVisualHost::prepareArtwork(const oto::visual::ImagePtr &image,QSize size,bool fill,bool circular) {
    owner();if(!image)return {};
    oto::visual::ImageNode node;const auto fit=image->image.size().scaled(size,fill?Qt::KeepAspectRatioByExpanding:Qt::KeepAspectRatio);node.box=QRect(QPoint((size.width()-fit.width())/2,(size.height()-fit.height())/2),fit);node.image=image;
    auto frame=engine_.prepare({size,{node}});if(!circular)return frame;QImage masked=frame->image;QImage mask(size,QImage::Format_RGBA8888_Premultiplied);mask.fill(Qt::transparent);{QPainter painter(&mask);painter.setRenderHint(QPainter::Antialiasing);painter.setBrush(Qt::white);painter.setPen(Qt::NoPen);painter.drawEllipse(QRectF(QPointF{},QSizeF(size)));}{QPainter painter(&masked);painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);painter.drawImage(0,0,mask);}return engine_.prepareRaster(std::move(masked));
}
oto::visual::FramePtr TrackVisualHost::prepareProArtwork(const oto::visual::ImagePtr &image,const TrackLayout &layout) {
    if(!layout.proPreset)return prepareArtwork(image,layout.artworkBox.size());
    if(!image)return {};
    const auto geometry=proCoverGeometry(layout.proPreset,layout.canvas);
    auto filled=prepareArtwork(image,geometry.box.size(),true);
    QImage pixels(geometry.box.size(),QImage::Format_RGBA8888_Premultiplied);pixels.fill(Qt::transparent);
    {QPainter painter(&pixels);painter.setRenderHint(QPainter::SmoothPixmapTransform);
        if(layout.proPreset==5||layout.proPreset==13){QTransform transform;const double w=filled->image.width(),h=filled->image.height();const QPolygonF source{QPointF(0,0),QPointF(w,0),QPointF(w,h),QPointF(0,h)};if(!QTransform::quadToQuad(source,geometry.quad,transform))throw std::runtime_error("Invalid Urban cover transform");painter.setTransform(transform);}
        painter.drawImage(0,0,filled->image);painter.resetTransform();
        // Extend one pixel beneath the front rim, avoiding fractional-scale seams.
        QImage mask=geometry.mask.copy();for(int y=0;y<mask.height();++y)for(int x=0;x<mask.width();++x){int alpha=0;for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)if(x+dx>=0&&y+dy>=0&&x+dx<mask.width()&&y+dy<mask.height())alpha=std::max(alpha,geometry.mask.pixelColor(x+dx,y+dy).alpha());mask.setPixelColor(x,y,QColor(255,255,255,alpha>=250?255:alpha));}
        painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);painter.drawImage(0,0,mask);}
    return engine_.prepareRaster(std::move(pixels));
}
oto::visual::VisualStats TrackVisualHost::stats() const {owner(); return engine_.stats();}
void TrackVisualHost::clear() {owner();normalCache_.clear();proCache_.clear();engine_.clear();}
} // namespace playing::visual

