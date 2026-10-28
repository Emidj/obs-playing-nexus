// SPDX-License-Identifier: GPL-2.0-or-later
#include "playing/visual/preset-art.hpp"
#include <QJsonObject>
#include <QJsonArray>
#include <QLinearGradient>
#include <QRadialGradient>
#include <cmath>
namespace playing::visual::normal {
namespace {
[[noreturn]] void invalid(){throw std::invalid_argument("Invalid bounded preset art recipe");}
double number(const QJsonValue &v,double low,double high){if(!v.isDouble())invalid();const double n=v.toDouble();if(!std::isfinite(n)||n<low||n>high)invalid();return n;}
QJsonObject object(const QJsonValue &v,std::initializer_list<const char*> keys){if(!v.isObject())invalid();auto o=v.toObject();if(o.size()!=static_cast<qsizetype>(keys.size()))invalid();for(auto key:keys)if(!o.contains(QLatin1String(key)))invalid();return o;}
QJsonArray array(const QJsonValue &v,int size){if(!v.isArray()||v.toArray().size()!=size)invalid();return v.toArray();}
ArtScalar scalar(const QJsonValue &v,bool inRepeat,bool allowCondition=true){
    ArtScalar result;if(v.isDouble()){result.terms[0]=number(v,-4096,4096);return result;}
    if(!v.isObject()||v.toObject().isEmpty()||v.toObject().size()>9)invalid();
    const std::array<QString,8> names{QStringLiteral("offset"),QStringLiteral("h"),QStringLiteral("ty"),QStringLiteral("th"),QStringLiteral("ay"),QStringLiteral("ah"),QStringLiteral("cy"),QStringLiteral("index")};
    const auto values=v.toObject();for(auto it=values.begin();it!=values.end();++it){
        if(it.key()==QLatin1String("stepBelow")){
            if(!allowCondition)invalid();auto o=object(it.value(),{"value","threshold","below","otherwise"});auto condition=std::make_shared<ArtCondition>();
            condition->test=scalar(o["value"],inRepeat,false).terms;condition->threshold=number(o["threshold"],-4096,4096);
            condition->below=number(o["below"],-4096,4096);condition->otherwise=number(o["otherwise"],-4096,4096);result.condition=std::move(condition);continue;
        }
        bool found=false;for(std::size_t i=0;i<names.size();++i)if(it.key()==names[i]){if(i==7&&!inRepeat)invalid();result.terms[i]=number(it.value(),-4096,4096);found=true;break;}if(!found)invalid();
    }
    return result;
}
ArtPoint point(const QJsonValue &v,bool loop){auto a=array(v,2);return {scalar(a[0],loop),scalar(a[1],loop)};}
QColor color(const QJsonValue &v){auto a=array(v,4);std::array<int,4> c;for(int i=0;i<4;++i){auto n=number(a[i],0,255);if(n!=std::floor(n))invalid();c[static_cast<std::size_t>(i)]=static_cast<int>(n);}return QColor(c[0],c[1],c[2],c[3]);}
ArtBrush brush(const QJsonValue &v,bool loop){
    if(!v.isObject()||!v.toObject()["kind"].isString())invalid();const auto kind=v.toObject()["kind"].toString();ArtBrush b;
    if(kind==QLatin1String("none")){object(v,{"kind"});return b;}
    if(kind==QLatin1String("solid")){auto o=object(v,{"kind","color"});b.kind=ArtBrush::Kind::Solid;b.color=color(o["color"]);return b;}
    QJsonObject o;
    if(kind==QLatin1String("linear")){o=object(v,{"kind","start","end","stops"});b.kind=ArtBrush::Kind::Linear;b.start=point(o["start"],loop);b.end=point(o["end"],loop);}
    else if(kind==QLatin1String("radial")){o=object(v,{"kind","center","radius","stops"});b.kind=ArtBrush::Kind::Radial;b.start=point(o["center"],loop);b.radius=scalar(o["radius"],loop);}
    else invalid();
    if(!o["stops"].isArray())invalid();const auto stops=o["stops"].toArray();if(stops.size()<2||stops.size()>8)invalid();double previous=-1;
    for(auto value:stops){auto s=object(value,{"at","color"});auto at=number(s["at"],0,1);if(at<=previous)invalid();previous=at;b.stops.push_back({at,color(s["color"])});}
    if(b.stops.front().at!=0||b.stops.back().at!=1)invalid();return b;
}
ArtLayer layer(const QJsonValue &v,bool loop,std::size_t &budget){
    if(!v.isArray()||v.toArray().isEmpty()||v.toArray().size()>256)invalid();ArtLayer result;
    for(const auto value:v.toArray()){
        if(!value.isObject()||!value.toObject()["op"].isString())invalid();const auto op=value.toObject()["op"].toString();ArtCommand c;QJsonObject o;
        if(op==QLatin1String("repeat")){
            if(loop)invalid();o=object(value,{"op","count","maximum","body"});c.kind=ArtCommand::Kind::Repeat;c.count=scalar(o["count"],false);
            const double maximum=number(o["maximum"],1,512);if(maximum!=std::floor(maximum))invalid();c.maximum=static_cast<int>(maximum);
            std::size_t childBudget=0;c.children=layer(o["body"],true,childBudget);budget+=1+childBudget*static_cast<std::size_t>(c.maximum);if(budget>1024)invalid();result.push_back(std::move(c));continue;
        }
        if(op==QLatin1String("rect")){o=object(value,{"op","box","radius","fill","stroke","width"});c.kind=ArtCommand::Kind::Rect;const auto box=array(o["box"],4);for(int i=0;i<4;++i)c.box[static_cast<std::size_t>(i)]=scalar(box[i],loop);c.radius=scalar(o["radius"],loop);}
        else if(op==QLatin1String("ellipse")){o=object(value,{"op","center","radius","fill","stroke","width"});c.kind=ArtCommand::Kind::Ellipse;c.start=point(o["center"],loop);c.radius=scalar(o["radius"],loop);}
        else if(op==QLatin1String("line")){o=object(value,{"op","start","end","fill","stroke","width"});c.kind=ArtCommand::Kind::Line;c.start=point(o["start"],loop);c.end=point(o["end"],loop);}
        else if(op==QLatin1String("path")){
            o=object(value,{"op","segments","fill","stroke","width"});c.kind=ArtCommand::Kind::Path;
            if(!o["segments"].isArray()||o["segments"].toArray().isEmpty()||o["segments"].toArray().size()>128)invalid();
            bool open=false;for(auto s:o["segments"].toArray()){
                if(!s.isArray()||s.toArray().isEmpty()||!s.toArray()[0].isString())invalid();const auto parts=s.toArray();const auto kind=parts[0].toString();int count=0;ArtSegment segment;
                if(kind==QLatin1String("M")){count=2;segment.kind='M';open=true;}
                else if(kind==QLatin1String("L")&&open){count=2;segment.kind='L';}
                else if(kind==QLatin1String("C")&&open){count=6;segment.kind='C';}
                else if(kind==QLatin1String("Z")&&open){segment.kind='Z';open=false;}else invalid();
                if(parts.size()!=count+1)invalid();for(int i=1;i<parts.size();++i)segment.values.push_back(scalar(parts[i],loop));c.segments.push_back(std::move(segment));
            }
        }else invalid();
        c.fill=brush(o["fill"],loop);if(!o["stroke"].isNull())c.stroke=color(o["stroke"]);c.width=number(o["width"],.1,64);
        if(c.kind==ArtCommand::Kind::Line&&(c.fill.kind!=ArtBrush::Kind::None||!c.stroke.isValid()))invalid();
        if(++budget>1024)invalid();result.push_back(std::move(c));
    }return result;
}
using Context=std::array<double,8>;
double evaluate(const ArtScalar &s,const Context &context){double n=0;for(std::size_t i=0;i<s.terms.size();++i)n+=s.terms[i]*context[i];
    if(s.condition){double test=0;for(std::size_t i=0;i<s.terms.size();++i)test+=s.condition->test[i]*context[i];if(!std::isfinite(test)||std::abs(test)>8192)invalid();n+=test<s.condition->threshold?s.condition->below:s.condition->otherwise;}
    if(!std::isfinite(n)||std::abs(n)>8192)invalid();return n;}
QPointF evaluate(const ArtPoint &p,const Context &c){return {evaluate(p.x,c),evaluate(p.y,c)};}
QBrush evaluate(const ArtBrush &b,const Context &c){
    if(b.kind==ArtBrush::Kind::None)return Qt::NoBrush;if(b.kind==ArtBrush::Kind::Solid)return b.color;
    if(b.kind==ArtBrush::Kind::Linear){const auto a=evaluate(b.start,c),z=evaluate(b.end,c);if(a==z)invalid();QLinearGradient g(a,z);for(const auto &s:b.stops)g.setColorAt(s.at,s.color);return g;}
    const double radius=evaluate(b.radius,c);if(radius<=0||radius>4096)invalid();QRadialGradient g(evaluate(b.start,c),radius);for(const auto &s:b.stops)g.setColorAt(s.at,s.color);return g;
}
void draw(QPainter &p,const ArtLayer &commands,Context context,std::size_t &remaining,bool loop=false){
    for(const auto &c:commands){
        if(remaining==0)invalid();--remaining;
        if(c.kind==ArtCommand::Kind::Repeat){if(loop||c.maximum<1||c.maximum>512)invalid();const double count=std::ceil(evaluate(c.count,context));if(count<0||count>c.maximum)invalid();for(int i=0;i<static_cast<int>(count);++i){context[7]=i;draw(p,c.children,context,remaining,true);}context[7]=0;continue;}
        p.setBrush(evaluate(c.fill,context));p.setPen(c.stroke.isValid()?QPen(c.stroke,c.width):QPen(Qt::NoPen));
        if(c.kind==ArtCommand::Kind::Rect){QRectF box(evaluate(c.box[0],context),evaluate(c.box[1],context),evaluate(c.box[2],context),evaluate(c.box[3],context));const double radius=evaluate(c.radius,context);if(radius<0||radius>4096)invalid();if(radius)p.drawRoundedRect(box,radius,radius);else p.drawRect(box);}
        else if(c.kind==ArtCommand::Kind::Ellipse){const double radius=evaluate(c.radius,context);if(radius<0||radius>4096)invalid();p.drawEllipse(evaluate(c.start,context),radius,radius);}
        else if(c.kind==ArtCommand::Kind::Line)p.drawLine(evaluate(c.start,context),evaluate(c.end,context));
        else {QPainterPath path;for(const auto &s:c.segments){
            const std::size_t expected=s.kind=='M'||s.kind=='L'?2:s.kind=='C'?6:0;
            if((!expected&&s.kind!='Z')||s.values.size()!=expected)invalid();
            std::array<double,6> v{};for(std::size_t i=0;i<s.values.size();++i)v[i]=evaluate(s.values[i],context);
            switch(s.kind){case 'M':path.moveTo(v[0],v[1]);break;case 'L':path.lineTo(v[0],v[1]);break;case 'C':path.cubicTo(v[0],v[1],v[2],v[3],v[4],v[5]);break;default:path.closeSubpath();break;}
        }p.drawPath(path);}
    }
}
}
std::array<ArtLayer,2> parsePresetArt(const QJsonValue &value){auto o=object(value,{"background","ornament"});std::size_t budget=0;auto background=layer(o["background"],false,budget);budget=0;auto ornament=layer(o["ornament"],false,budget);return {std::move(background),std::move(ornament)};}
void paintPresetArt(QPainter &painter,const ArtLayer &commands,const NativeLayout &layout){
    if(layout.canvas.width()<400||layout.canvas.width()>1600||layout.canvas.height()<1||layout.canvas.height()>2048||commands.empty())invalid();
    const double scale=layout.canvas.width()/500.0;Context context{1,layout.canvas.height()/scale,layout.rows[1].box.y()/scale,layout.rows[1].box.height()/scale,layout.rows[2].box.y()/scale,layout.rows[2].box.height()/scale,0,0};context[6]=context[2]+context[3]/2;
    std::size_t remaining=1024;painter.save();try{painter.scale(scale,scale);painter.setRenderHint(QPainter::Antialiasing);draw(painter,commands,context,remaining);}catch(...){painter.restore();throw;}painter.restore();
}
}
