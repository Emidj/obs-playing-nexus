// SPDX-License-Identifier: MIT
#include "playing/visual/terminal-layout.hpp"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <iostream>
#include <limits>
using namespace playing::visual::normal;
namespace {
unsigned checks=0;
void check(bool value,const char *why){++checks;if(!value)throw std::runtime_error(why);}
void compare(const QRectF &native,const QJsonValue &value){
    const auto a=value.toArray();check(a.size()==4,"Original rectangle schema");
    const double actual[]={native.x(),native.y(),native.width(),native.height()};
    for(int i=0;i<4;++i){
        if(std::abs(actual[i]-a[i].toDouble())>=.0001)
            std::cerr<<"Rectangle component "<<i<<" native="<<actual[i]<<" original="<<a[i].toDouble()<<'\n';
        check(std::abs(actual[i]-a[i].toDouble())<.0001,"Original Terminal geometry parity");
    }
}
}
int main(int argc,char **argv){
 QCoreApplication application(argc,argv);
 try{
    check(argc==2,"Original Terminal layouts required");QFile file(QString::fromUtf8(argv[1]));
    check(file.open(QIODevice::ReadOnly),"Open Terminal reference");
    const auto document=QJsonDocument::fromJson(file.readAll());check(document.isArray(),"Layout reference schema");
    const auto cases=document.array();check(cases.size()==36,"All widths/gaps/metadata required");
    for(const auto value:cases){const auto original=value.toObject();const auto rows=original["rows"].toArray();
        TerminalMetrics metrics;
        for(int i=0;i<3;++i)metrics.widths[static_cast<std::size_t>(i)]=rows[i].toObject()["full"].toDouble();
        const auto layout=terminalLayout(original["width"].toInt(),original["gap"].toInt(),metrics);
        compare(layout.panel,original["panel"]);compare(layout.header,original["top"]);
        compare(layout.main,original["main"]);compare(layout.prompt,original["prompt"]);
        compare(layout.cursor,original["cursor"]);compare(layout.bottom,original["bottom"]);
        for(int i=0;i<3;++i){
            compare(layout.rows[static_cast<std::size_t>(i)],rows[i].toObject()["box"]);
            check(std::abs(layout.overflow[static_cast<std::size_t>(i)]-
                  std::max(0.0,rows[i].toObject()["full"].toDouble()-rows[i].toObject()["client"].toInt()))<.0001,
                  "Original fractional scroll distance");
        }
        for(int i=0;i<12;++i)compare(layout.bars[static_cast<std::size_t>(i)],original["bars"].toArray()[i]);
        check(std::abs(layout.scale-original["scale"].toDouble())<.0001,"Original root scale");
    }
    for(int width:{399,1601}){bool rejected=false;try{terminalLayout(width,12,{});}catch(const std::invalid_argument &){rejected=true;}check(rejected,"Invalid output width rejected");}
    // Independent outputs from the original Lua height contract, including
    // the gap=80 case omitted by the DOM rectangle-only reference bank.
    for(const auto &sample:std::array<std::array<int,3>,7>{{
        {400,0,210},{400,12,210},{400,80,346},{720,12,378},
        {720,80,514},{1280,24,696},{1600,80,976}}})
        check(terminalLayout(sample[0],sample[1],{}).canvas==QSize(sample[0],sample[2]),"Original Lua Terminal output canvas");
    TerminalMetrics invalid;invalid.widths[1]=std::numeric_limits<double>::quiet_NaN();
    bool rejected=false;try{terminalLayout(720,12,invalid);}catch(const std::invalid_argument &){rejected=true;}check(rejected,"NaN metrics rejected");
    std::cout<<"PASS "<<checks<<" Terminal geometry checks; font metrics supplied by original, not native pixel certification\n";
    return 0;
 }catch(const std::exception &error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
