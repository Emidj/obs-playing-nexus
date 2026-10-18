// SPDX-License-Identifier: MIT
#include "playing/visual/animation-profiles.hpp"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <set>
static void resources(){Q_INIT_RESOURCE(animation_profiles);}
namespace playing::visual::animation {
namespace {
double number(const QJsonValue &value,double fallback=0){if(value.isUndefined())return fallback;if(!value.isDouble()||!std::isfinite(value.toDouble()))throw std::invalid_argument("Animation number");return value.toDouble();}
void fields(const QJsonObject &object,std::initializer_list<const char*> allowed){for(auto it=object.begin();it!=object.end();++it){bool found=false;for(const auto key:allowed)found=found||it.key()==QLatin1String(key);if(!found)throw std::invalid_argument("Unknown animation field");}}
}
std::vector<Profile> parseProfiles(const QByteArray &bytes) {
    if(bytes.size()>65536)throw std::invalid_argument("Animation document budget");
    QJsonParseError error;auto doc=QJsonDocument::fromJson(bytes,&error);if(error.error!=QJsonParseError::NoError||!doc.isObject())throw std::invalid_argument("Animation JSON");
    const auto root=doc.object();fields(root,{"format","version","profiles"});
    if(root["format"]!="obs-playing.animations"||number(root["version"])!=1||!root["profiles"].isArray())throw std::invalid_argument("Animation schema");
    const auto items=root["profiles"].toArray();if(items.isEmpty()||items.size()>16)throw std::invalid_argument("Animation profile budget");
    std::vector<Profile> result;std::set<int> settings;
    for(const auto item:items){
        if(!item.isObject())throw std::invalid_argument("Animation profile");const auto object=item.toObject();fields(object,{"setting","name","layers"});
        const double setting=number(object["setting"],-1);const auto name=object["name"].toString();
        if(setting<0||setting>4096||std::floor(setting)!=setting||!settings.insert(static_cast<int>(setting)).second||name.isEmpty()||name.size()>80||!object["layers"].isArray())throw std::invalid_argument("Animation identity");
        Profile profile{static_cast<int>(setting),name,{}};std::set<std::size_t> targets;
        const auto layers=object["layers"].toArray();if(layers.size()>4)throw std::invalid_argument("Animation layer budget");
        for(const auto layer:layers){
            if(!layer.isObject())throw std::invalid_argument("Animation layer");const auto o=layer.toObject();fields(o,{"target","duration","delay","loop","keys"});
            const auto target=o["target"].toString();const QStringList names={"label","title","artist","artwork"};const int index=names.indexOf(target);
            if(index<0||!targets.insert(static_cast<std::size_t>(index)).second||!o["keys"].isArray())throw std::invalid_argument("Animation target");
            Loop loop;const auto kind=o["loop"].toString();if(kind=="once")loop=Loop::Once;else if(kind=="repeat")loop=Loop::Repeat;else if(kind=="pingpong")loop=Loop::PingPong;else throw std::invalid_argument("Animation loop");
            std::vector<Key> keys;const auto array=o["keys"].toArray();if(array.size()>32)throw std::invalid_argument("Animation key budget");
            for(const auto entry:array){if(!entry.isObject())throw std::invalid_argument("Animation key");const auto k=entry.toObject();fields(k,{"at","x","y","opacity","scale","angle","ease"});Key key;key.at=number(k["at"],-1);key.value={number(k["x"]),number(k["y"]),number(k["opacity"],1),number(k["scale"],1),number(k["angle"])};
                if((!k["ease"].isUndefined()&&!k["ease"].isString())||(index!=3&&key.value.angle!=0))throw std::invalid_argument("Clipped text rotation unsupported");
                const auto ease=k["ease"].toString("linear");if(ease=="linear")key.ease=Ease::Linear;else if(ease=="cosine")key.ease=Ease::Cosine;else if(ease=="step")key.ease=Ease::Step;else throw std::invalid_argument("Animation ease");keys.push_back(key);}
            profile.program.clips[static_cast<std::size_t>(index)]=Clip(number(o["duration"]),number(o["delay"]),loop,std::move(keys));
        }
        if(profile.setting==0&&!layers.isEmpty())throw std::invalid_argument("Default motion must preserve designs");result.push_back(std::move(profile));
    }
    if(!settings.count(0))throw std::invalid_argument("Animation default missing");return result;
}
const std::vector<Profile>& profiles(){static const auto value=[] {resources();QFile file(":/playing/animations-v1.json");if(!file.open(QIODevice::ReadOnly))throw std::runtime_error("Animation resource missing");return parseProfiles(file.read(65537));}();return value;}
const Program& program(int setting){for(const auto &profile:profiles())if(profile.setting==setting)return profile.program;throw std::invalid_argument("Animation profile missing");}
}
