// SPDX-License-Identifier: GPL-2.0-or-later
#include "playing/visual/track-visual.hpp"
#include "playing/visual/preset-definition.hpp"
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <iostream>
namespace playing::visual::normal::test_reference {void paintPhase13aArt(QPainter&,NativePreset,const NativeLayout&,bool);}
using namespace playing::visual::normal;
int main(int argc,char **argv){
    QGuiApplication app(argc,argv);unsigned checks=0;
    const auto check=[&](bool value,const char *message){++checks;if(!value)throw std::runtime_error(message);};
    try{
        check(argc==2,"Catalogue path required");QFile file(QString::fromUtf8(argv[1]));check(file.open(QIODevice::ReadOnly),"Catalogue open");
        const auto root=QJsonDocument::fromJson(file.readAll()).object();QJsonObject oblique,terminal;
        for(const auto &v:root["presets"].toArray()){auto p=v.toObject();if(p["id"]==QStringLiteral("oblique"))oblique=p["art"].toObject();if(p["id"]==QStringLiteral("terminal"))terminal=p["art"].toObject();}
        const auto rejects=[&](const QJsonValue &value){bool rejected=false;try{parsePresetArt(value);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"Invalid art recipe accepted");};
        rejects(QJsonObject{});auto unknown=oblique;unknown["script"]="anything";rejects(unknown);
        const auto commandReject=[&](const char *key,const QJsonValue &value){auto art=oblique;auto background=art["background"].toArray();auto command=background[0].toObject();command[key]=value;background[0]=command;art["background"]=background;rejects(art);};
        commandReject("op","shader");commandReject("box",QJsonArray{1,2,3});commandReject("box",QJsonArray{QJsonObject{{"unknown",1}},2,3,4});
        commandReject("box",QJsonArray{QJsonObject{{"index",1}},2,3,4});commandReject("radius",4097);commandReject("width",0);
        commandReject("fill",QJsonObject{{"kind","solid"},{"color",QJsonArray{255,0,0,256}}});
        const QJsonObject lastStop{{"at",1},{"color",QJsonArray{0,0,0,255}}},firstStop{{"at",0},{"color",QJsonArray{0,0,0,255}}};
        commandReject("fill",QJsonObject{{"kind","linear"},{"start",QJsonArray{0,0}},{"end",QJsonArray{1,1}},{"stops",QJsonArray{lastStop,firstStop}}});
        auto paths=oblique;auto background=paths["background"].toArray();auto polygon=background[1].toObject();polygon["segments"]=QJsonArray{QJsonArray{"L",0,0}};background[1]=polygon;paths["background"]=background;rejects(paths);
        auto loops=terminal;auto commands=loops["background"].toArray();auto repeat=commands[2].toObject();repeat["maximum"]=513;commands[2]=repeat;loops["background"]=commands;rejects(loops);
        repeat=terminal["background"].toArray()[2].toObject();repeat["body"]=QJsonArray{repeat};commands[2]=repeat;loops["background"]=commands;rejects(loops);
        playing::visual::TrackVisualHost host;
        const auto track=std::make_shared<const playing::core::TrackState>(playing::core::TrackState{u8"Jóga 🎵 日本語","Björk"});
        unsigned comparisons=0;
        for(const auto &entry:normalCatalogue())if(redesigned(entry.preset))for(int width:{400,720,1600}){
            const auto prepared=host.prepareNormal(entry.preset,track,width,width==400?0:width==720?12:80);
            for(int pass=0;pass<2;++pass){
                QImage reference(prepared->layout.canvas,QImage::Format_RGBA8888_Premultiplied);reference.fill(Qt::transparent);QPainter p(&reference);
                test_reference::paintPhase13aArt(p,entry.preset,prepared->layout,pass==1);p.end();
                const auto &actual=pass?prepared->ornaments[0].pixels->image:prepared->background->image;
                if(actual!=reference)throw std::runtime_error(std::string(entry.id)+" at "+std::to_string(width)+" layer "+std::to_string(pass)+" differs from frozen13-A pixels");
                check(true,"Art pixel parity");++comparisons;
            }
        }
        // Lens threshold must retain both corner orientations, including data
        // layouts shorter than the bundled NORMAL typography produces.
        auto lens=host.prepareNormal(NativePreset::Lens,track,720);auto shortLayout=lens->layout;shortLayout.canvas.setHeight(170);
        QImage expected(shortLayout.canvas,QImage::Format_RGBA8888_Premultiplied),actual(shortLayout.canvas,QImage::Format_RGBA8888_Premultiplied);expected.fill(Qt::transparent);actual.fill(Qt::transparent);
        QPainter oldPainter(&expected);test_reference::paintPhase13aArt(oldPainter,NativePreset::Lens,shortLayout,false);oldPainter.end();
        QPainter newPainter(&actual);paintPresetArt(newPainter,presetDefinition(NativePreset::Lens).art[0],shortLayout);newPainter.end();check(actual==expected,"Lens short-canvas conditional changed");
        auto changed=oblique;auto bg=changed["background"].toArray();auto first=bg[0].toObject();first["fill"]=QJsonObject{{"kind","solid"},{"color",QJsonArray{255,0,0,255}}};bg[0]=first;changed["background"]=bg;
        auto changedArt=parsePresetArt(changed);auto normal=host.prepareNormal(NativePreset::Oblique,track,720);QImage edited(normal->layout.canvas,QImage::Format_RGBA8888_Premultiplied);edited.fill(Qt::transparent);QPainter editPainter(&edited);paintPresetArt(editPainter,changedArt[0],normal->layout);editPainter.end();check(edited!=normal->background->image,"Data brush edit ignored by painter");
        // Invalid evaluated coordinates cannot allocate a large raster or run
        // unbounded loops, even when a typed fixture bypasses JSON validation.
        auto invalidLayer=presetDefinition(NativePreset::Oblique).art[0];invalidLayer[0].box[0].terms[1]=4096;
        QPainter bounded(&edited);bool failed=false;try{paintPresetArt(bounded,invalidLayer,normal->layout);}catch(const std::invalid_argument&){failed=true;}bounded.end();check(failed,"Evaluated coordinate budget bypassed");
        check(track->title==u8"Jóga 🎵 日本語","Art preparation mutated Core metadata");
        std::cout<<"PASS "<<checks<<" art validation/edit/bounds checks; exact pixel comparisons="<<comparisons<<"\n";return 0;
    }catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
