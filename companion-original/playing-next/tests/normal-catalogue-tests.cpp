// SPDX-License-Identifier: GPL-2.0-or-later
#include "playing/visual/track-visual.hpp"
#include "playing/visual/preset-definition.hpp"
#include <QGuiApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <set>
#include <iostream>
#include <cmath>
using namespace playing::visual::normal;
namespace playing::visual::normal {
PreparedNormalPtr preparePhase13bClassic(oto::visual::VisualEngine&,NativePreset,TrackPtr,int,const QString&,int);
NativeLayout phase13c1Layout(NativePreset,int,const std::array<QSize,3>&,int);
oto::visual::Scene phase13c1Background(NativePreset,const NativeLayout&,QColor,double);
}
int main(int argc,char **argv) {
    QGuiApplication app(argc,argv);unsigned checks=0;
    const auto check=[&](bool value,const char*message){++checks;if(!value)throw std::runtime_error(message);};
    try {
        check(argc==2,"Original catalogue required");QFile file(QString::fromUtf8(argv[1]));check(file.open(QIODevice::ReadOnly),"Original catalogue open");
        const auto original=QJsonDocument::fromJson(file.readAll()).object();std::set<QString> ids;
        for(const auto &item:original["catalogue"].toArray())ids.insert(item.toObject()["id"].toString());
        check(ids.size()==23,"Original 23 NORMAL required");
        playing::visual::TrackVisualHost host;
        oto::visual::VisualEngine referenceEngine;
        const auto shortTrack=std::make_shared<const playing::core::TrackState>(playing::core::TrackState{"Lipstick","Alaia & Gallo"});
        const auto unicode=std::make_shared<const playing::core::TrackState>(playing::core::TrackState{u8"Jóga 🎵 日本語","Björk"});
        const auto longTrack=std::make_shared<const playing::core::TrackState>(playing::core::TrackState{std::string(4096,'W'),std::string(2048,'M')});
        for(std::size_t i=0;i<normalCatalogue().size();++i) {
            const auto &entry=normalCatalogue()[i];check(ids.erase(QString::fromUtf8(entry.id.c_str()))==1,"Lost or duplicated NORMAL ID");
            check(presetFromSetting(static_cast<int>(i+1))==entry.preset,"Stable preset setting");
            check(presetFromId(entry.id)==entry.preset,"Original preset ID resolution");
            const auto &base=presetDefinition(entry.preset);
            const auto unchanged=customizedDefinition(base,{});
            check(unchanged.text[1].family==base.text[1].family&&unchanged.text[1].color==base.text[1].color&&unchanged.text[1].pixelSize==base.text[1].pixelSize,"Empty overrides changed defaults");
            TextOverrides overrides;overrides[0].family=QStringLiteral("Arial");overrides[0].pixelSize=24;overrides[0].bold=false;overrides[0].italic=true;overrides[0].color=QColor(20,240,80,128);
            const auto customized=customizedDefinition(base,overrides);
            check(customized.text[0].family==base.text[0].family&&customized.text[2].color==base.text[2].color,"Override changed label or artist");
            check(customized.text[1].family=="Arial"&&customized.text[1].pixelSize==24&&customized.text[1].weight==QFont::Normal&&customized.text[1].italic&&customized.text[1].color.alpha()==128,"Explicit text override lost");
            const auto customFrame=prepareNativeNormal(referenceEngine,entry.preset,shortTrack,720,QStringLiteral("NOW PLAYING"),12,overrides);
            check(customFrame&&!customFrame->text[1].empty(),"Customized NORMAL raster missing");
            const auto defaultFrame=prepareNativeNormal(referenceEngine,entry.preset,shortTrack,720,QStringLiteral("NOW PLAYING"),12);
            check(customFrame->text[1][0].pixels->image!=defaultFrame->text[1][0].pixels->image,"Override did not change title pixels");
            check(customFrame->text[0][0].pixels->image==defaultFrame->text[0][0].pixels->image,"Override changed label pixels");
            check(presetDefinition(entry.preset).text[1].family==base.text[1].family,"Customization mutated catalogue");
            const auto sharedCustom=host.prepareNormal(entry.preset,shortTrack,720,12,overrides);
            check(sharedCustom==host.prepareNormal(entry.preset,shortTrack,720,12,overrides),"Custom cache failed to share");
            check(sharedCustom!=host.prepareNormal(entry.preset,shortTrack,720,12),"Custom cache reused default pixels");
            overrides[0].effects.outline=2;overrides[0].effects.shadow=3;
            const auto effected=host.prepareNormal(entry.preset,shortTrack,720,12,overrides);
            check(effected!=sharedCustom&&effected->text[1][0].pixels->image!=sharedCustom->text[1][0].pixels->image,"Effects ignored by classic cache/raster");
            for(const auto &tile:effected->text[1])check(tile.pixels->image.width()<=2048,"Effect tile exceeds GPU size");
            auto invalid=overrides;invalid[0].pixelSize=129;bool rejected=false;try{customizedDefinition(base,invalid);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"Oversized custom font accepted");
            for(const int width:{400,720,1600}) {
                const int gap=width==400?0:width==720?12:80;
                const auto track=width==400?shortTrack:width==720?unicode:longTrack;
                PreparedNormalPtr frame;
                try{frame=host.prepareNormal(entry.preset,track,width,gap);}
                catch(const std::exception &error){throw std::runtime_error(std::string(entry.id)+" at "+std::to_string(width)+": "+error.what());}
                const auto same=host.prepareNormal(entry.preset,track,width,gap);
                if(!redesigned(entry.preset)){
                    const auto before=preparePhase13bClassic(referenceEngine,entry.preset,track,width,QStringLiteral("NOW PLAYING"),gap);
                    check(frame->layout.canvas==before->layout.canvas,"Classic canvas changed");
                    check(frame->background->image==before->background->image,"Classic background pixels changed");
                    check(frame->animatedDecoration==before->animatedDecoration&&frame->solidRects.size()==before->solidRects.size(),"Classic decoration count/state changed");
                    for(std::size_t rect=0;rect<frame->solidRects.size();++rect){const auto &a=frame->solidRects[rect],&b=before->solidRects[rect];
                        check(a.box==b.box&&a.color==b.color&&a.travel==b.travel&&a.period==b.period,"Classic panel/movement changed");
                    }
                    for(std::size_t row=0;row<3;++row){
                        check(frame->layout.rows[row].box==before->layout.rows[row].box&&frame->layout.rows[row].overflow==before->layout.rows[row].overflow,"Classic layout/scroll changed");
                        check(frame->text[row].size()==before->text[row].size(),"Classic tile count changed");
                        for(std::size_t tile=0;tile<frame->text[row].size();++tile)
                            check(frame->text[row][tile].pixels->image==before->text[row][tile].pixels->image,"Classic glyph pixels changed");
                    }
                }
                check(frame==same&&frame->track==track,"Native prepared cache/state identity");
                check(frame->layout.canvas.width()==width&&frame->layout.canvas.height()<=2048,"Native canvas bounds");
                for(std::size_t row=0;row<3;++row){
                    check(QRectF(QPointF{},frame->layout.canvas).contains(frame->layout.rows[row].box),"Native text outside canvas");
                    check(!frame->text[row].empty(),"Native text layer missing");
                    bool ink=false;
                    for(const auto &tile:frame->text[row]){
                        check(tile.pixels&&tile.pixels->image.width()<=2048&&tile.pixels->image.height()<=256,"Native glyph budget");
                        const auto &image=tile.pixels->image;for(int y=0;y<image.height()&&!ink;++y)for(int x=0;x<image.width()&&!ink;++x)ink=image.pixelColor(x,y).alpha()>0;
                    }
                    check(ink,"Native text produced no ink");
                }
                if(redesigned(entry.preset))check(frame->background&&!frame->ornaments.empty()&&frame->animatedDecoration&&!frame->legacyTextBlend,"Incomplete native redesigned scene");
                if(width==1600)check(frame->layout.rows[1].overflow>0&&frame->layout.rows[2].overflow>0,"Long text lost scrolling");
            }
        }
        check(ids.empty(),"NORMAL catalogue incomplete");
        QFile data(QStringLiteral(":/playing/normal-v5.json"));check(data.open(QIODevice::ReadOnly),"Definition resource open");
        auto extension=QJsonDocument::fromJson(data.readAll()).object();auto definitions=extension["presets"].toArray();auto extra=definitions[3].toObject();
        extra["id"]="fixture-extra";extra["name"]="Fixture extra";extra["setting"]=27;definitions.append(extra);extension["presets"]=definitions;
        auto extraAliases=extension["aliases"].toArray();extraAliases.append(QJsonObject{{"id","fixture-alias"},{"name","Fixture alias"},{"target","fixture-extra"},{"setting",28}});extension["aliases"]=extraAliases;
        const auto extended=parsePresetCatalogue(QJsonDocument(extension).toJson(QJsonDocument::Compact));
        check(extended.presets.size()==24&&extended.aliases.size()==4&&extended.presets.back().setting==27,"New data-only definition rejected");
        QFile legacy(QStringLiteral(":/playing/normal-compatibility-v1.json"));check(legacy.open(QIODevice::ReadOnly),"Extension compatibility resource open");validatePresetCompatibility(extended,legacy.readAll());
        const auto extraFrame=prepareRedesignedDefinition(referenceEngine,extended.presets.back(),unicode,720,QStringLiteral("NOW PLAYING"),12);
        const auto templateFrame=host.prepareNormal(NativePreset::Oblique,unicode,720);
        check(extraFrame->background->image==templateFrame->background->image&&extraFrame->layout.canvas==templateFrame->layout.canvas,"New ID bypasses existing generic renderer");
        check(extraFrame->ornaments[0].pixels->image==templateFrame->ornaments[0].pixels->image,"New data-only ornament differs");
        check(normalPreviewHeight(1,400)==166&&normalPreviewHeight(2,1600)==152&&normalPreviewHeight(24,720)==316,"Saved preview geometry changed");
        const std::array<QSize,3> metrics{QSize(97,18),QSize(1200,42),QSize(900,26)};
        for(const auto preset:{NativePreset::Prism,NativePreset::Air,NativePreset::Cantilever})for(const int width:{400,401,500,721,999,1600})for(const int gap:{0,12,80}){
            const auto actual=nativeLayout(preset,width,metrics,gap),previous=phase13c1Layout(preset,width,metrics,gap);
            check(actual.canvas==previous.canvas,"Classic arbitrary-width canvas differs");
            for(std::size_t row=0;row<3;++row)check(actual.rows[row].box==previous.rows[row].box&&actual.rows[row].overflow==previous.rows[row].overflow,"Classic arbitrary-width geometry differs");
            for(const double opacity:{0.0,.53,1.0}){
                const QColor accent(13,88,211);const auto a=referenceEngine.prepare(nativeBackground(preset,actual,accent,opacity));
                const auto b=referenceEngine.prepare(phase13c1Background(preset,previous,accent,opacity));
                check(a->image==b->image,"Classic accent/opacity background pixel mismatch");
            }
        }
        auto changedClassic=presetDefinition(NativePreset::Prism).classic;
        const auto originalLayout=classicLayout(changedClassic,720,metrics,12);changedClassic.top+=10;
        const auto changedLayout=classicLayout(changedClassic,720,metrics,12);
        check(changedLayout.rows[0].box.y()==originalLayout.rows[0].box.y()+10,"Classic layout data edit ignored");
        const auto originalBackground=referenceEngine.prepare(classicBackground(changedClassic,changedLayout,QColor(182,228,143),.92));
        changedClassic.rectangles[0].accent=false;changedClassic.rectangles[0].color=Qt::red;
        check(referenceEngine.prepare(classicBackground(changedClassic,changedLayout,QColor(182,228,143),.92))->image!=originalBackground->image,"Classic rectangle data edit ignored");
        changedClassic.rectangles[0].box[0][0].first=99;bool badCoordinate=false;
        try{classicBackground(changedClassic,changedLayout,QColor(182,228,143),.92);}catch(const std::invalid_argument&){badCoordinate=true;}
        check(badCoordinate,"Invalid typed classic scalar accepted");
        std::string greekTitle;for(int i=0;i<2048;++i)greekTitle+=u8"ΐ";
        const auto greek=std::make_shared<const playing::core::TrackState>(playing::core::TrackState{greekTitle,"Artist"});
        for(const auto preset:{NativePreset::Oblique,NativePreset::Rupture,NativePreset::Terminal}){
            const auto frame=host.prepareNormal(preset,greek,720);
            check(frame&&!frame->text[1].empty()&&frame->track->title==greekTitle,"Maximum Unicode uppercase expansion lost track");
        }
        const auto aliases=original["aliases"].toObject();check(aliases.size()==3,"Original aliases required");
        for(std::size_t i=0;i<normalAliases().size();++i){
            const auto &alias=normalAliases()[i];check(aliases.contains(alias.id.c_str()),"Alias missing");
            check(presetFromId(alias.id)==alias.preset,"Original alias ID resolution");
            const auto aliasFrame=host.prepareNormal(presetFromSetting(static_cast<int>(i+24)),shortTrack,720);
            check(aliasFrame==host.prepareNormal(alias.preset,shortTrack,720),"Alias duplicates scene/cache");
            const auto target=aliases[alias.id.c_str()].toString();bool found=false;for(const auto &entry:normalCatalogue())if(target==QString::fromUtf8(entry.id.c_str()))found=entry.preset==alias.preset;
            check(found,"Alias resolves to wrong original design");
        }
        bool rejected=false;try{presetFromSetting(27);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"Unknown preset accepted");
        check(shortTrack->title=="Lipstick"&&unicode->title==u8"Jóga 🎵 日本語"&&longTrack->title.size()==4096,"Presentation mutated Core text");
        host.clear();check(host.stats().bytes==0,"Cache clear retains visual bytes");
        std::cout<<"PASS "<<checks<<" complete native NORMAL catalogue/alias/Unicode/bounds checks\n";return 0;
    }catch(const std::exception &error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
