// SPDX-License-Identifier: GPL-2.0-or-later
#include "playing/visual/preset-definition.hpp"
#include "playing/visual/normal-prepared.hpp"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <iostream>
using namespace playing::visual::normal;
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);unsigned checks=0;
    const auto check=[&](bool result,const char *message){++checks;if(!result)throw std::runtime_error(message);};
    try {
        check(argc==2,"Source catalogue path required");QFile file(QString::fromUtf8(argv[1]));check(file.open(QIODevice::ReadOnly),"Source catalogue open");
        const auto bytes=file.readAll();const auto original=QJsonDocument::fromJson(bytes).object();
        const auto parsed=parsePresetCatalogue(bytes);const auto &bundled=bundledPresetCatalogue();
        QFile compatibility(QStringLiteral(":/playing/normal-compatibility-v1.json"));check(compatibility.open(QIODevice::ReadOnly),"Compatibility resource open");const auto legacy=compatibility.readAll();
        validatePresetCompatibility(parsed,legacy);check(true,"Original saved settings preserved");
        for(std::size_t i=0;i<normalCatalogue().size();++i){
            const auto &a=parsed[i],&b=bundled[i];
            check(a.id==b.id&&a.name==b.name&&a.nativeRecipe==b.nativeRecipe,"Source/resource catalogue differs");
            check(a.setting==static_cast<int>(i+1)&&a.setting==b.setting&&a.previewHeight==b.previewHeight&&a.scalePreviewHeight==b.scalePreviewHeight,"Saved numeric settings/preview changed");
            check(&presetDefinition(normalCatalogue()[i].preset)==&b,"Definition not immutable/canonical");
            if(a.nativeRecipe){
                check(a.travel==b.travel&&a.period==b.period&&a.pulse==b.pulse&&a.left==b.left&&a.available==b.available,"Embedded layout/motion differs");
                for(std::size_t row=0;row<3;++row)check(a.text[row].family==b.text[row].family&&a.text[row].color==b.text[row].color&&a.text[row].pixelSize==b.text[row].pixelSize&&a.text[row].weight==b.text[row].weight&&a.text[row].italic==b.text[row].italic&&a.text[row].uppercase==b.text[row].uppercase,"Embedded typography differs");
            }else{
                check(a.scaleClassicText==b.scaleClassicText&&a.classicOutline==b.classicOutline,"Embedded classic flags differ");
                for(std::size_t row=0;row<3;++row)check(a.text[row].family==b.text[row].family&&a.text[row].color==b.text[row].color&&a.text[row].pixelSize==b.text[row].pixelSize&&a.text[row].weight==b.text[row].weight,"Embedded classic typography differs");
            }
        }
        const auto rejects=[&](const QByteArray &json){bool failed=false;try{parsePresetCatalogue(json);}catch(const std::invalid_argument&){failed=true;}check(failed,"Invalid preset catalogue accepted");};
        rejects({});rejects(QByteArray(262145,' '));rejects(bytes.left(bytes.size()/2));rejects("[]");
        const auto rootReject=[&](const char *field,const QJsonValue &value){auto root=original;root[field]=value;rejects(QJsonDocument(root).toJson(QJsonDocument::Compact));};
        rootReject("version",1);rootReject("version",3);rootReject("version",2.5);rootReject("version","2");rootReject("format","other");rootReject("executable","script");
        rootReject("aliases",QJsonObject{{"neon","terminal"},{"broadcast","oblique"},{"editorial","chrome"}});
        auto presets=original["presets"].toArray();auto duplicate=presets;duplicate[1]=duplicate[0];rootReject("presets",duplicate);
        const auto compatibilityReject=[&](QJsonObject root){bool failed=false;try{validatePresetCompatibility(parsePresetCatalogue(QJsonDocument(root).toJson(QJsonDocument::Compact)),legacy);}catch(const std::invalid_argument&){failed=true;}check(failed,"Compatibility loss accepted");};
        auto dropped=original;auto fewer=presets;fewer.removeLast();dropped["presets"]=fewer;compatibilityReject(dropped);
        auto renamed=original;auto named=presets;auto name=named[0].toObject();name["name"]="Changed";named[0]=name;renamed["presets"]=named;compatibilityReject(renamed);
        auto rebound=original;auto aliasList=original["aliases"].toArray();auto alias=aliasList[0].toObject();alias["target"]="terminal";aliasList[0]=alias;rebound["aliases"]=aliasList;compatibilityReject(rebound);
        auto reordered=original;QJsonArray reversed;for(qsizetype i=presets.size();i-->0;)reversed.append(presets[i]);reordered["presets"]=reversed;
        const auto stable=parsePresetCatalogue(QJsonDocument(reordered).toJson(QJsonDocument::Compact));validatePresetCompatibility(stable,legacy);
        for(std::size_t i=0;i<parsed.presets.size();++i)check(stable[i].id==parsed[i].id&&stable[i].setting==parsed[i].setting,"File order changes saved preset mapping");
        const auto classicReject=[&](const char *field,const QJsonValue &value){auto root=original;auto list=presets;auto entry=list[0].toObject();entry[field]=value;list[0]=entry;root["presets"]=list;rejects(QJsonDocument(root).toJson(QJsonDocument::Compact));};
        classicReject("scaleText",1);classicReject("outline","true");classicReject("text",QJsonArray{});
        const auto classicDefinitionReject=[&](const char *field,const QJsonValue &value){auto definition=presets[0].toObject()["classic"].toObject();definition[field]=value;classicReject("classic",definition);};
        classicDefinitionReject("script","anything");classicDefinitionReject("roundExtents",1);classicDefinitionReject("rectangles",QJsonArray{});
        const auto rectangleReject=[&](const char *field,const QJsonValue &value){auto rectangles=presets[0].toObject()["classic"].toObject()["rectangles"].toArray();auto first=rectangles[0].toObject();first[field]=value;rectangles[0]=first;classicDefinitionReject("rectangles",rectangles);};
        rectangleReject("color","#xxxxxx");rectangleReject("alpha",1.1);rectangleReject("moving",true);rectangleReject("opacityScaled",1);
        rectangleReject("box",QJsonArray{QJsonArray{QJsonArray{"unknown",1}},QJsonArray{},QJsonArray{},QJsonArray{}});
        rectangleReject("box",QJsonArray{QJsonArray{QJsonArray{"w",2049}},QJsonArray{QJsonArray{"offset",0}},QJsonArray{QJsonArray{"offset",1}},QJsonArray{QJsonArray{"offset",1}}});
        auto badMotion=presets[0].toObject()["classic"].toObject()["motion"].toObject();badMotion["period"]=0;classicDefinitionReject("motion",badMotion);
        badMotion["period"]=5500;badMotion["travel"]=1;classicDefinitionReject("motion",badMotion);
        auto badLayout=presets[0].toObject()["classic"].toObject()["layout"].toObject();badLayout["rows"]=QJsonArray{};classicDefinitionReject("layout",badLayout);
        badLayout=presets[0].toObject()["classic"].toObject()["layout"].toObject();badLayout["top"]=-1;classicDefinitionReject("layout",badLayout);
        badLayout["top"]=20;auto badRows=badLayout["rows"].toArray();auto badRow=badRows[0].toObject();badRow["left"]=399;badRow["right"]=20;badRows[0]=badRow;badLayout["rows"]=badRows;classicDefinitionReject("layout",badLayout);
        const auto classicRowReject=[&](const char *field,const QJsonValue &value){auto rows=presets[0].toObject()["text"].toArray();auto row=rows[1].toObject();row[field]=value;rows[1]=row;classicReject("text",rows);};
        classicRowReject("cellHeight",8.5);classicRowReject("cellHeight",129);classicRowReject("family","Bad\nFont");classicRowReject("family",QString(32,'X'));classicRowReject("bold",1);classicRowReject("italic",1);classicRowReject("color","#xxxxxx");
        auto scaledRoot=original;auto scaledList=presets;auto scaledEntry=scaledList[2].toObject();auto scaledRows=scaledEntry["text"].toArray();auto scaledRow=scaledRows[1].toObject();
        for(const int height:{9,81}){scaledRow["cellHeight"]=height;scaledRows[1]=scaledRow;scaledEntry["text"]=scaledRows;scaledList[2]=scaledEntry;scaledRoot["presets"]=scaledList;rejects(QJsonDocument(scaledRoot).toJson(QJsonDocument::Compact));}
        auto missing=presets;missing.removeLast();rootReject("presets",QJsonArray{});
        const auto entryReject=[&](const char *field,const QJsonValue &value){auto root=original;auto list=presets;auto entry=list[3].toObject();entry[field]=value;list[3]=entry;root["presets"]=list;rejects(QJsonDocument(root).toJson(QJsonDocument::Compact));};
        entryReject("name","");entryReject("id","bad id");entryReject("renderer","script");entryReject("shader","anything");
        entryReject("setting",0);entryReject("setting",1);entryReject("setting",4.5);entryReject("setting",4097);
        entryReject("preview",QJsonObject{{"height",1},{"scaled",true}});
        const auto layoutReject=[&](const char *field,const QJsonValue &value){auto layout=presets[3].toObject()["layout"].toObject();layout[field]=value;entryReject("layout",layout);};
        layoutReject("left",500);layoutReject("available",0);layoutReject("center",1);layoutReject("bottom",101);layoutReject("top",-1);layoutReject("rowGap",QJsonValue());
        const auto textReject=[&](const char *field,const QJsonValue &value){auto text=presets[3].toObject()["text"].toArray();auto row=text[1].toObject();row[field]=value;text[1]=row;entryReject("text",text);};
        textReject("family",QString(129,'X'));textReject("family","Bad\nFont");textReject("pixelSize",129);textReject("weight",700.5);textReject("color","#ff00ff00");textReject("color","#xxxxxx");textReject("italic",1);textReject("uppercase","true");
        const auto motionReject=[&](const char *field,const QJsonValue &value){auto motion=presets[3].toObject()["motion"].toObject();motion[field]=value;entryReject("motion",motion);};
        motionReject("period",0);motionReject("pulse",1.1);motionReject("x",33);motionReject("y",-33);
        // An edited definition is parsed as data without changing the immutable
        // live catalogue; later checkpoints can publish a validated revision.
        auto changed=original;auto list=presets;auto entry=list[3].toObject();auto text=entry["text"].toArray();auto title=text[1].toObject();title["color"]="#123456";title["pixelSize"]=50;text[1]=title;entry["text"]=text;list[3]=entry;changed["presets"]=list;
        auto motion=entry["motion"].toObject();motion["period"]=1000;entry["motion"]=motion;
        auto layout=entry["layout"].toObject();layout["left"]=50;entry["layout"]=layout;
        list[3]=entry;changed["presets"]=list;
        const auto edited=parsePresetCatalogue(QJsonDocument(changed).toJson(QJsonDocument::Compact));
        check(edited[3].text[1].color==QColor("#123456")&&edited[3].text[1].pixelSize==50,"Definition ignores data edits");
        check(presetDefinition(NativePreset::Oblique).text[1].color!=edited[3].text[1].color,"Parsed edit mutated live catalogue");
        check(edited[3].left==50&&edited[3].period==1000,"Layout/motion data edit ignored");
        auto classicEdit=original;auto classicList=presets;auto classicEntry=classicList[0].toObject();auto classicText=classicEntry["text"].toArray();auto classicTitle=classicText[1].toObject();
        classicTitle["family"]="Arial";classicTitle["cellHeight"]=40;classicTitle["color"]="#123456";classicTitle["bold"]=false;classicTitle["italic"]=true;
        classicText[1]=classicTitle;classicEntry["text"]=classicText;classicEntry["outline"]=true;
        auto classicGeometry=classicEntry["classic"].toObject();auto classicLayoutData=classicGeometry["layout"].toObject();classicLayoutData["top"]=35;classicGeometry["layout"]=classicLayoutData;classicEntry["classic"]=classicGeometry;
        classicList[0]=classicEntry;classicEdit["presets"]=classicList;
        const auto classicParsed=parsePresetCatalogue(QJsonDocument(classicEdit).toJson(QJsonDocument::Compact));
        check(classicParsed[0].text[1].family=="Arial"&&classicParsed[0].text[1].pixelSize==40&&classicParsed[0].text[1].color==QColor("#123456")&&classicParsed[0].text[1].weight==QFont::Normal&&classicParsed[0].text[1].italic&&classicParsed[0].classicOutline,"Classic data edit ignored");
        check(presetDefinition(NativePreset::Prism).text[1].family=="Segoe UI"&&!presetDefinition(NativePreset::Prism).classicOutline,"Classic edit mutated live catalogue");
        check(classicParsed[0].classic.top==35&&presetDefinition(NativePreset::Prism).classic.top==20,"Classic geometry data edit ignored or mutated live catalogue");
        check(&presetDefinition(presetFromId("neon"))==&presetDefinition(NativePreset::Orbit),"Alias definition duplicated");
        bool oversized=false;try{redesignedLayout(NativePreset::Oblique,720,{{QSize(100,1024),QSize(100,1024),QSize(100,1024)}},12);}catch(const std::invalid_argument&){oversized=true;}
        check(oversized,"Oversized native recipe canvas accepted");
        std::cout<<"PASS "<<checks<<" versioned preset catalogue/resource/rejection checks\n";return 0;
    }catch(const std::exception &error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
