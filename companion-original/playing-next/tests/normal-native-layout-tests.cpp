// SPDX-License-Identifier: MIT
#include "playing/visual/normal-native-layout.hpp"
#include "visual/visual-engine.hpp"
#include <QGuiApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <iostream>
#include <stdexcept>
using namespace playing::visual::normal;
namespace {
unsigned checks=0;
void check(bool ok,const char *why) {++checks;if(!ok)throw std::runtime_error(why);}
}
int main(int argc,char **argv) {
    QGuiApplication application(argc,argv);
    try {
        check(argc==2,"Original GDI evidence required");
        QFile file(QString::fromUtf8(argv[1]));check(file.open(QIODevice::ReadOnly),"Original evidence open");
        const auto document=QJsonDocument::fromJson(file.readAll());check(document.isArray(),"Original evidence schema");
        const auto cases=document.array();check(cases.size()==18,"All original GDI cases required");
        oto::visual::VisualEngine engine;
        const std::array<QString,3> keys{QStringLiteral("label"),QStringLiteral("title"),QStringLiteral("artist")};
        for(const auto value:cases) {
            const auto original=value.toObject();const auto metrics=original["metrics"].toObject();
            const auto id=original["preset"].toString();check(id=="studio"||id=="minimal","Original preset ID");
            const auto preset=id=="studio"?NativePreset::Prism:NativePreset::Air;
            std::array<QSize,3> sizes;
            for(std::size_t i=0;i<keys.size();++i){const auto size=metrics[keys[i]].toObject();sizes[i]=QSize(size["width"].toInt(),size["height"].toInt());}
            const auto layout=nativeLayout(preset,original["width"].toInt(),sizes);
            check(layout.canvas==QSize(original["width"].toInt(),original["height"].toInt()),"Original canvas parity");
            for(std::size_t i=0;i<keys.size();++i) {
                const auto row=original["rows"].toObject()[keys[i]].toObject();const auto &native=layout.rows[i];
                check(native.box==QRectF(row["x"].toDouble(),row["y"].toDouble(),row["width"].toDouble(),row["height"].toDouble()),"Original geometry parity");
                check(native.overflow==row["overflow"].toInt(),"Original overflow parity");
            }
            const auto scene=nativeBackground(preset,layout);check(scene.nodes.size()==(preset==NativePreset::Prism?6u:4u),"Authored ornament count");
            const auto first=engine.prepare(scene);const auto second=engine.prepare(scene);
            check(first&&first==second,"Shared engine background reuse");
        }
        const auto fractional=nativeLayout(NativePreset::Air,721,{QSize(80,16),QSize(96,36),QSize(96,22)});
        check(fractional.rows[0].box.x()==320.5,"Fractional centering preserved");
        bool rejected=false;try{nativeLayout(NativePreset::Prism,399,{QSize(78,14),QSize(94,36),QSize(94,20)});}catch(const std::invalid_argument &){rejected=true;}
        check(rejected,"Invalid width rejected");
        std::cout<<"PASS "<<checks<<" native NORMAL geometry/shared-engine checks; font and OBS pixel parity pending\n";
        return 0;
    }catch(const std::exception &error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
