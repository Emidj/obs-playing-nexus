// SPDX-License-Identifier: GPL-2.0-or-later
#include "playing/visual/preset-definition.hpp"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include <QResource>
#include <set>
#include <cmath>
#include <algorithm>
static void initializePlayingPresets() { Q_INIT_RESOURCE(normal_presets); }
namespace playing::visual::normal {
namespace {
[[noreturn]] void invalid(const char *message) {throw std::invalid_argument(message);}
QJsonObject object(const QJsonValue &value,std::initializer_list<const char*> keys) {
    if(!value.isObject())invalid("Preset field must be object");
    const auto result=value.toObject();
    if(result.size()!=static_cast<qsizetype>(keys.size()))invalid("Preset fields missing or unknown");
    for(const auto *key:keys)if(!result.contains(QLatin1String(key)))invalid("Preset fields missing or unknown");
    return result;
}
QString string(const QJsonValue &value,int limit=128) {
    if(!value.isString())invalid("Preset field must be string");
    const auto result=value.toString();
    if(result.isEmpty()||result.size()>limit)invalid("Preset string bounds");
    for(const auto character:result)if(character.unicode()<32||character.unicode()==127)invalid("Preset string control character");
    return result;
}
QString identifier(const QJsonValue &value){
    const auto result=string(value,64);
    for(const auto c:result)if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_'))invalid("Invalid preset identifier");
    return result;
}
double number(const QJsonValue &value,double low,double high) {
    if(!value.isDouble())invalid("Preset field must be number");
    const double result=value.toDouble();
    if(!std::isfinite(result)||result<low||result>high)invalid("Preset number bounds");
    return result;
}
bool boolean(const QJsonValue &value) {
    if(!value.isBool())invalid("Preset field must be boolean");return value.toBool();
}
int setting(const QJsonValue &value){const auto result=number(value,1,4096);if(result!=std::floor(result))invalid("Preset setting must be integer");return static_cast<int>(result);}
QColor color(const QJsonValue &value) {
    const auto text=string(value,7);
    if(text.size()!=7||text[0]!=QChar('#'))invalid("Preset color requires #RRGGBB");
    for(int i=1;i<7;++i)if(!((text[i]>='0'&&text[i]<='9')||(text[i]>='a'&&text[i]<='f')||(text[i]>='A'&&text[i]<='F')))invalid("Invalid preset color");
    return QColor(text);
}
ClassicScalar classicScalar(const QJsonValue &value){
    static const std::array<const char*,11> variables{"offset","w","h","s","ly","lw","lh","ty","th","ay","ah"};
    if(!value.isArray()||value.toArray().isEmpty()||value.toArray().size()>8)invalid("Classic scalar requires 1..8 terms");
    ClassicScalar result;
    for(const auto &term:value.toArray()){
        if(!term.isArray()||term.toArray().size()!=2)invalid("Classic term requires variable/coefficient");
        const auto pair=term.toArray();const auto name=string(pair[0]);std::size_t index=variables.size();
        for(std::size_t i=0;i<variables.size();++i)if(name==QLatin1String(variables[i])){index=i;break;}
        if(index==variables.size())invalid("Unknown classic variable");
        result.emplace_back(index,number(pair[1],-2048,2048));
    }
    return result;
}
ClassicDefinition classicDefinition(const QJsonValue &value){
    const auto root=object(value,{"layout","rectangles","roundExtents","roundQuads","motion"});ClassicDefinition result;
    result.roundExtents=boolean(root["roundExtents"]);result.roundQuads=boolean(root["roundQuads"]);
    const auto layout=object(root["layout"],{"scaled","center","top","bottom","minimumHeight","rows"});
    result.scaled=boolean(layout["scaled"]);result.center=boolean(layout["center"]);
    result.top=number(layout["top"],0,128);result.bottom=number(layout["bottom"],0,128);result.minimumHeight=number(layout["minimumHeight"],0,512);
    if(!layout["rows"].isArray()||layout["rows"].toArray().size()!=3)invalid("Classic layout requires three rows");
    for(int i=0;i<3;++i){const auto row=object(layout["rows"].toArray()[i],{"left","right","available","gap"});auto &target=result.rows[static_cast<std::size_t>(i)];
        target.left=number(row["left"],0,result.scaled?499:399);target.right=number(row["right"],0,499);
        target.available=number(row["available"],0,500);target.gap=number(row["gap"],0,100);
        if(target.available&&target.left+target.available>(result.scaled?500:400))invalid("Classic row outside minimum canvas");
        if(!target.available&&target.left+target.right>(result.scaled?498:399))invalid("Classic margins leave no text area");
    }
    const auto motion=object(root["motion"],{"travel","period"});result.travel=number(motion["travel"],0,32);result.period=number(motion["period"],200,60000);
    if(!root["rectangles"].isArray()||root["rectangles"].toArray().isEmpty()||root["rectangles"].toArray().size()>32)invalid("Classic requires 1..32 rectangles");
    const auto rectangles=root["rectangles"].toArray();int moving=0;
    for(int i=0;i<rectangles.size();++i){const auto rectangle=object(rectangles[i],{"box","color","alpha","opacityScaled","moving"});ClassicRectangle target;
        if(!rectangle["box"].isArray()||rectangle["box"].toArray().size()!=4)invalid("Classic rectangle box requires four scalars");
        for(int j=0;j<4;++j)target.box[static_cast<std::size_t>(j)]=classicScalar(rectangle["box"].toArray()[j]);
        const auto tint=string(rectangle["color"]);target.accent=tint==QStringLiteral("accent");if(!target.accent)target.color=color(rectangle["color"]);
        target.alpha=number(rectangle["alpha"],0,1);target.opacityScaled=boolean(rectangle["opacityScaled"]);target.moving=boolean(rectangle["moving"]);
        if(target.moving){++moving;if(i!=rectangles.size()-1)invalid("Moving classic rectangle must be last");}
        result.rectangles.push_back(std::move(target));
    }
    if(moving!=(result.travel?1:0))invalid("Classic motion/rectangle mismatch");
    return result;
}
}
PresetCatalogue parsePresetCatalogue(const QByteArray &json) {
    if(json.isEmpty()||json.size()>262144)invalid("Preset catalogue byte bounds");
    QJsonParseError error;const auto document=QJsonDocument::fromJson(json,&error);
    if(error.error!=QJsonParseError::NoError||!document.isObject())invalid("Malformed preset catalogue JSON");
    const auto root=object(document.object(),{"format","version","presets","aliases"});
    if(string(root["format"])!=QStringLiteral("obs-playing.preset-catalogue")||number(root["version"],5,5)!=5)invalid("Unsupported preset catalogue version");
    if(!root["presets"].isArray()||root["presets"].toArray().isEmpty()||root["presets"].toArray().size()>128)invalid("Preset catalogue requires 1..128 definitions");
    PresetCatalogue result;std::set<QString> ids;std::set<int> settings;
    for(const auto &value:root["presets"].toArray()) {
        if(!value.isObject())invalid("Preset entry must be object");
        const auto entry=value.toObject();const auto id=identifier(entry["id"]);
        if(!ids.insert(id).second)invalid("Duplicated preset ID");
        result.presets.emplace_back();auto &definition=result.presets.back();definition.id=id;definition.name=string(entry["name"]);
        definition.setting=setting(entry["setting"]);if(!settings.insert(definition.setting).second)invalid("Duplicated preset setting");
        const auto preview=object(entry["preview"],{"height","scaled"});
        const auto previewSize=number(preview["height"],1,640);if(previewSize!=std::floor(previewSize))invalid("Preset preview height must be integer");
        definition.previewHeight=static_cast<int>(previewSize);definition.scalePreviewHeight=boolean(preview["scaled"]);
        if(definition.scalePreviewHeight&&definition.previewHeight<2)invalid("Scaled preview height vanishes at minimum width");
        const auto renderer=string(entry["renderer"]);if(renderer!=QStringLiteral("native-recipe")&&renderer!=QStringLiteral("classic"))invalid("Unknown preset renderer");
        definition.nativeRecipe=renderer==QStringLiteral("native-recipe");
        if(!definition.nativeRecipe){
            object(entry,{"id","name","setting","preview","renderer","text","scaleText","outline","classic"});
            definition.classic=classicDefinition(entry["classic"]);
            definition.scaleClassicText=boolean(entry["scaleText"]);
            definition.classicOutline=boolean(entry["outline"]);
            if(!entry["text"].isArray()||entry["text"].toArray().size()!=3)invalid("Classic requires three text layers");
            const auto rows=entry["text"].toArray();
            for(int i=0;i<3;++i){
                const auto layer=object(rows[i],{"family","cellHeight","bold","italic","color"});
                auto &row=definition.text[static_cast<std::size_t>(i)];
                row.family=string(layer["family"],31);row.color=color(layer["color"]);
                row.pixelSize=number(layer["cellHeight"],definition.scaleClassicText?10:8,definition.scaleClassicText?80:128);
                if(row.pixelSize!=std::floor(row.pixelSize))invalid("Classic cell height must be integer");
                row.weight=boolean(layer["bold"])?QFont::Bold:QFont::Normal;
                row.italic=boolean(layer["italic"]);
            }
            continue;
        }
        object(entry,{"id","name","setting","preview","renderer","layout","text","motion","art"});
        definition.art=parsePresetArt(entry["art"]);
        const auto layout=object(entry["layout"],{"left","available","center","top","rowGap","bottom"});
        definition.left=number(layout["left"],0,500);definition.available=number(layout["available"],1,500);
        if(definition.left+definition.available>500)invalid("Preset layout outside reference canvas");
        definition.center=boolean(layout["center"]);definition.top=number(layout["top"],0,100);
        definition.rowGap=number(layout["rowGap"],0,100);definition.bottom=number(layout["bottom"],0,100);
        if(!entry["text"].isArray()||entry["text"].toArray().size()!=3)invalid("Preset requires three text layers");
        const auto text=entry["text"].toArray();
        for(int i=0;i<3;++i){
            const auto layer=object(text[i],{"family","pixelSize","weight","color","italic","uppercase"});auto &row=definition.text[static_cast<std::size_t>(i)];
            row.family=string(layer["family"]);row.pixelSize=number(layer["pixelSize"],8,128);row.color=color(layer["color"]);
            const double weight=number(layer["weight"],QFont::Thin,QFont::Black);
            if(weight!=std::floor(weight))invalid("Preset font weight must be integer");row.weight=static_cast<QFont::Weight>(static_cast<int>(weight));
            row.italic=boolean(layer["italic"]);row.uppercase=boolean(layer["uppercase"]);
        }
        const auto motion=object(entry["motion"],{"x","y","period","pulse"});
        definition.travel={number(motion["x"],-32,32),number(motion["y"],-32,32)};
        definition.period=number(motion["period"],200,60000);definition.pulse=number(motion["pulse"],0,1);
    }
    if(!root["aliases"].isArray()||root["aliases"].toArray().size()>128)invalid("Preset alias budget");
    for(const auto &value:root["aliases"].toArray()){
        const auto alias=object(value,{"id","name","target","setting"});PresetAliasDefinition definition;
        definition.id=identifier(alias["id"]);definition.name=string(alias["name"]);definition.target=identifier(alias["target"]);definition.setting=setting(alias["setting"]);
        if(!ids.insert(definition.id).second||!settings.insert(definition.setting).second)invalid("Duplicated alias ID/setting");
        bool found=false;for(const auto &entry:result.presets)if(definition.target==entry.id)found=true;
        if(!found)invalid("Alias target must be canonical preset");
        result.aliases.push_back(std::move(definition));
    }
    std::sort(result.presets.begin(),result.presets.end(),[](const auto &a,const auto &b){return a.setting<b.setting;});
    std::sort(result.aliases.begin(),result.aliases.end(),[](const auto &a,const auto &b){return a.setting<b.setting;});
    return result;
}
void validatePresetCompatibility(const PresetCatalogue &catalogue,const QByteArray &json){
    if(json.isEmpty()||json.size()>32768)invalid("Compatibility byte bounds");
    QJsonParseError error;const auto document=QJsonDocument::fromJson(json,&error);
    if(error.error!=QJsonParseError::NoError||!document.isObject())invalid("Malformed compatibility JSON");
    const auto root=object(document.object(),{"format","version","presets","aliases"});
    if(string(root["format"])!=QStringLiteral("obs-playing.preset-compatibility")||number(root["version"],1,1)!=1)invalid("Unsupported compatibility version");
    if(!root["presets"].isArray()||root["presets"].toArray().size()!=23||!root["aliases"].isArray()||root["aliases"].toArray().size()!=3)invalid("Compatibility requires original 23/3");
    std::set<QString> ids;std::set<int> settings;
    for(const auto &value:root["presets"].toArray()){
        const auto p=object(value,{"id","name","setting","renderer","preview"});const auto id=identifier(p["id"]);const int key=setting(p["setting"]);
        if(!ids.insert(id).second||!settings.insert(key).second)invalid("Duplicate compatibility record");
        const auto name=string(p["name"]),renderer=string(p["renderer"]);const auto preview=object(p["preview"],{"height","scaled"});
        const auto height=number(preview["height"],1,640);const bool scaled=boolean(preview["scaled"]);bool found=false;
        for(const auto &entry:catalogue.presets)if(entry.id==id){found=entry.setting==key&&entry.name==name&&(entry.nativeRecipe?QStringLiteral("native-recipe"):QStringLiteral("classic"))==renderer&&entry.previewHeight==height&&entry.scalePreviewHeight==scaled;break;}
        if(!found)invalid("Original preset compatibility changed");
    }
    for(const auto &value:root["aliases"].toArray()){
        const auto a=object(value,{"id","name","setting","target"});const auto id=identifier(a["id"]);const int key=setting(a["setting"]);
        if(!ids.insert(id).second||!settings.insert(key).second)invalid("Duplicate compatibility alias");
        const auto name=string(a["name"]),target=identifier(a["target"]);bool found=false;
        for(const auto &entry:catalogue.aliases)if(entry.id==id){found=entry.setting==key&&entry.name==name&&entry.target==target;break;}
        if(!found)invalid("Original alias compatibility changed");
    }
}
const PresetCatalogue &bundledPresetCatalogue() {
    static const PresetCatalogue catalogue=[] {
        initializePlayingPresets();QFile file(QStringLiteral(":/playing/normal-v5.json"));
        if(!file.open(QIODevice::ReadOnly))throw std::runtime_error("Bundled NORMAL catalogue unavailable");
        if(file.size()>262144)throw std::runtime_error("Bundled NORMAL catalogue oversized");
        auto parsed=parsePresetCatalogue(file.readAll());QFile compatibility(QStringLiteral(":/playing/normal-compatibility-v1.json"));
        if(!compatibility.open(QIODevice::ReadOnly)||compatibility.size()>32768)throw std::runtime_error("Bundled compatibility unavailable");
        validatePresetCompatibility(parsed,compatibility.readAll());return parsed;
    }();
    return catalogue;
}
const PresetDefinition &presetDefinition(NativePreset preset) {
    for(const auto &definition:bundledPresetCatalogue().presets)if(definition.setting-1==static_cast<int>(preset))return definition;
    throw std::invalid_argument("Unknown NORMAL definition");
}
const std::vector<PresetEntry> &normalCatalogue(){
    static const std::vector<PresetEntry> entries=[] {std::vector<PresetEntry> result;for(const auto &p:bundledPresetCatalogue().presets)result.push_back({p.id.toStdString(),p.name.toUtf8().toStdString(),static_cast<NativePreset>(p.setting-1),p.setting});return result;}();return entries;
}
const std::vector<PresetEntry> &normalAliases(){
    static const std::vector<PresetEntry> entries=[] {std::vector<PresetEntry> result;for(const auto &a:bundledPresetCatalogue().aliases){
        for(const auto &p:bundledPresetCatalogue().presets)if(p.id==a.target)result.push_back({a.id.toStdString(),a.name.toUtf8().toStdString(),static_cast<NativePreset>(p.setting-1),a.setting});
    }return result;}();return entries;
}
NativePreset presetFromSetting(int value){for(const auto &p:normalCatalogue())if(p.setting==value)return p.preset;for(const auto &a:normalAliases())if(a.setting==value)return a.preset;throw std::invalid_argument("Unknown preset setting");}
NativePreset presetFromId(std::string_view id){for(const auto &p:normalCatalogue())if(std::string_view(p.id)==id)return p.preset;for(const auto &a:normalAliases())if(std::string_view(a.id)==id)return a.preset;throw std::invalid_argument("Unknown preset ID");}
bool redesigned(NativePreset value){return presetDefinition(value).nativeRecipe;}
int normalPreviewHeight(int value,int width){const auto &p=presetDefinition(presetFromSetting(value));return p.scalePreviewHeight?static_cast<int>(p.previewHeight*width/500.0):p.previewHeight;}
}
