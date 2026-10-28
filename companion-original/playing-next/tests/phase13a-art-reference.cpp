// SPDX-License-Identifier: GPL-2.0-or-later
// Frozen checkpoint13-A oracle; test-only, excluded from the product.
#include "playing/visual/normal-native-layout.hpp"
#include <QPainter>
#include <QLinearGradient>
#include <QRadialGradient>
#include <cmath>
namespace playing::visual::normal::test_reference {
QPainterPath polygon(std::initializer_list<QPointF> points) {
    QPainterPath path;bool first=true;for(const auto &point:points){if(first){path.moveTo(point);first=false;}else path.lineTo(point);}path.closeSubpath();return path;
}
void paintPhase13aArt(QPainter &p,NativePreset preset,const NativeLayout &layout,bool ornament) {
    const double scale=layout.canvas.width()/500.0,h=layout.canvas.height()/scale;
    p.scale(scale,scale);p.setRenderHint(QPainter::Antialiasing);p.setPen(Qt::NoPen);
    const double ty=layout.rows[1].box.y()/scale,th=layout.rows[1].box.height()/scale;
    const double ay=layout.rows[2].box.y()/scale,ah=layout.rows[2].box.height()/scale;
    const double cy=ty+th/2;
    const auto rect=[&](QRectF box,QColor color,double radius=0){p.setBrush(color);p.setPen(Qt::NoPen);if(radius)p.drawRoundedRect(box,radius,radius);else p.drawRect(box);};
    const auto path=[&](const QPainterPath &shape,QColor color){p.setBrush(color);p.setPen(Qt::NoPen);p.drawPath(shape);};
    const auto line=[&](QPointF a,QPointF b,QColor color,double width=1){p.setPen(QPen(color,width));p.setBrush(Qt::NoBrush);p.drawLine(a,b);p.setPen(Qt::NoPen);};
    const auto ring=[&](QPointF center,double radius,QColor color,double width=1){p.setPen(QPen(color,width));p.setBrush(Qt::NoBrush);p.drawEllipse(center,radius,radius);p.setPen(Qt::NoPen);};
    const auto disc=[&](QPointF center,double radius,QColor color){p.setBrush(color);p.setPen(Qt::NoPen);p.drawEllipse(center,radius,radius);};
    const auto gradient=[&](QRectF box,QColor a,QColor b,double radius){QLinearGradient g(box.topLeft(),box.bottomRight());g.setColorAt(0,a);g.setColorAt(1,b);p.setBrush(g);p.setPen(Qt::NoPen);p.drawRoundedRect(box,radius,radius);};
    switch(preset) {
    case NativePreset::Oblique:
        if(!ornament){rect({28,ty-8,430,th+17},QColor("#d7ff39"));path(polygon({{458,ty-8},{479,ty-8},{458,ty+th+9}}),QColor("#d7ff39"));rect({28,ay-5,430,ah+14},QColor("#1c2915"));rect({28,20,285,22},QColor("#d7ff39"));line({28,ay+ah+12},{458,ay+ah+12},QColor("#d7ff39"),2);}
        else for(int i=0;i<3;++i)line({430.0+i*9,ay+3},{424.0+i*9,ay+ah-3},QColor("#d7ff39"),3);
        break;
    case NativePreset::Aura:
        if(!ornament){gradient({20,15,460,h-35},QColor("#e2c6ea"),QColor("#a9d5dc"),55);ring({87,cy},43,QColor(255,255,255,102),2);}
        else {QRadialGradient g(QPointF(73,cy-17),55);g.setColorAt(0,QColor("#fff8ff"));g.setColorAt(.35,QColor("#d69bf3"));g.setColorAt(.7,QColor("#8791d1"));g.setColorAt(1,QColor("#6ac6df"));p.setBrush(g);p.drawEllipse(QPointF(85,cy),38,38);for(int i=0;i<3;++i)disc({442.0+i*9,32},1.5,QColor("#805d9a"));}
        break;
    case NativePreset::Orbit:
        if(!ornament){gradient({15,15,470,h-35},QColor("#081625"),QColor("#09111d"),24);for(int i=0;i<3;++i)ring({85,cy},30+i*13,QColor("#3baafc"),i==2?1.5:1);line({145,ay+ah+14},{465,ay+ah+14},QColor("#54b9ff"),1.5);}
        else {disc({85,cy},13,QColor("#64d7ff"));disc({121,cy-38},5,QColor("#e4fcff"));}
        break;
    case NativePreset::Terminal:
        if(!ornament){rect({25,15,450,h-33},QColor("#071b13"));p.setBrush(Qt::NoBrush);p.setPen(QPen(QColor("#579c76"),1));p.drawRect(QRectF(25,15,450,h-33));p.setPen(Qt::NoPen);for(double y=19;y<h-20;y+=5)line({28,y},{472,y},QColor(120,224,160,11));line({42,ty-7},{458,ty-7},QColor("#407c5d"));for(int i=0;i<12;++i)rect({43.0+i*34,h-33,27,2},QColor(i<6?"#9bffc1":"#37624a"));}
        else {rect({36,cy-6,3,12},QColor("#a3fac4"));rect({450,25,6,6},QColor("#a3fac4"));}
        break;
    case NativePreset::Rupture:
        if(!ornament){path(polygon({{25,ty-5},{464,ty-19},{449,ty+th+15},{32,ty+th+25}}),QColor("#fff63b"));rect({45,17,142,23},QColor("#e9e4d7"));rect({37,ay-2,390,ah+9},QColor("#261c29"));}
        else {QPainterPath star;for(int i=0;i<20;++i){const double a=i*3.141592653589793/10,r=i%2?27:47;QPointF point(458+std::cos(a)*r,cy+std::sin(a)*r);if(i)star.lineTo(point);else star.moveTo(point);}star.closeSubpath();path(star,QColor("#ff49ad"));}
        break;
    case NativePreset::Dune:
        if(!ornament){gradient({20,15,460,h-35},QColor("#f2bf84"),QColor("#c67b60"),60);disc({415,cy},45,QColor(255,224,169,130));}
        else {for(int i=0;i<3;++i){QPainterPath dune;dune.moveTo(285+i*15,h-24);dune.cubicTo(335,h-100-i*8,425,h-115-i*5,479,h-110-i*8);dune.lineTo(479,h-30);dune.closeSubpath();path(dune,QColor(132,61,43,40+i*10));}}
        break;
    case NativePreset::Chrome:
        if(!ornament){gradient({22,15,456,h-35},QColor("#e8f5fc"),QColor("#8ba8bb"),12);rect({22,15,456,30},QColor("#2c435a"),7);for(int i=0;i<38;++i)line({45.0+i*10,h-30},{45.0+i*10,h-25},QColor("#567189"));}
        else for(int i=0;i<4;++i)path(polygon({{42.0+i*12,cy+30},{59.0+i*12,cy-30},{68.0+i*12,cy-30},{51.0+i*12,cy+30}}),QColor(i%2?"#5d849d":"#c9edfc"));
        break;
    case NativePreset::Monolith:
        if(!ornament){gradient({25,15,450,h-35},QColor("#292c32"),QColor("#101216"),2);rect({25,10,13,h-28},QColor("#ff773a"));for(int i=0;i<4;++i)rect({443.0+i*7,ty,2,th+20},QColor("#55585e"));}
        else rect({25,ty,13,24},QColor("#ffe5a7"));
        break;
    case NativePreset::Portal:
        if(!ornament){rect({140,30,337,h-57},QColor("#14144b"));for(int i=0;i<3;++i){p.setPen(QPen(QColor("#8b8bff"),i?1.5:3));p.setBrush(i?Qt::NoBrush:QBrush(QColor("#252778")));p.drawRect(QRectF(28+i*14,18+i*14,105-i*28,h-35-i*28));p.setPen(Qt::NoPen);}}
        else line({61,35},{61,h-43},QColor("#bcbaff"),2);
        break;
    case NativePreset::Beacon:
        if(!ornament){gradient({22,15,456,h-35},QColor("#402318"),QColor("#171c20"),9);for(int i=0;i<3;++i)disc({88,cy},53-i*13,QColor(237,135,46,22+i*18));rect({85,cy+35,6,h-65-cy},QColor("#ffaf60"));}
        else {disc({88,cy},23,QColor("#ef8943"));disc({88,cy},15,QColor("#ffc56c"));disc({88,cy},8,QColor("#fff3c0"));}
        break;
    case NativePreset::Laser:
        if(!ornament){path(polygon({{25,25},{450,25},{478,10},{478,h-30},{25,h-30}}),QColor("#06221b"));for(int i=0;i<5;++i)line({425.0+i*7,30},{405.0+i*7,ty+th},QColor("#1a6651"),1);}
        else line({23,h-25},{66,18},QColor("#b7ffe4"),3);
        break;
    case NativePreset::Matrix:
        if(!ornament){rect({20,15,460,h-35},QColor("#13243c"),28);line({26,h-19},{461,h-19},QColor("#5bcef1"),2);for(int x=0;x<7;++x)for(int y=0;y<7;++y)disc({44.0+x*12,35.0+y*(h-80)/6},2.7,QColor(83,190,211,210-x*24));}
        else for(int i=0;i<7;++i)disc({44.0+i*12,cy},2.7,QColor("#a9f1ff"));
        break;
    case NativePreset::Lotus:
        if(!ornament){rect({40,20,438,h-40},QColor("#efd0d8"),50);}
        else {QPainterPath leaf;leaf.moveTo(72,h-27);leaf.cubicTo(22,cy,42,28,85,12);leaf.cubicTo(127,cy-25,110,cy+50,72,h-27);path(leaf,QColor("#ad5b7c"));QPainterPath petal;petal.moveTo(78,h-39);petal.cubicTo(75,45,139,8,155,15);petal.cubicTo(146,95,133,h-53,78,h-39);path(petal,QColor("#d88ba6"));QPainterPath cream;cream.moveTo(106,h-44);cream.cubicTo(128,cy,150,cy-12,170,cy-7);cream.cubicTo(158,h-49,128,h-40,106,h-44);path(cream,QColor("#f9e6db"));}
        break;
    case NativePreset::Tide:
        if(!ornament){gradient({20,15,460,h-35},QColor("#d3efee"),QColor("#79c4c9"),48);rect({30,ay-3,440,ah+12},QColor("#218592"),10);}
        else {for(int i=0;i<2;++i){QPainterPath wave;wave.moveTo(310,h-34-i*15);wave.cubicTo(315,h-90,413,h-124,462,h-64-i*13);p.setBrush(Qt::NoBrush);p.setPen(QPen(QColor(i?"#e3ffff":"#3c99a4"),5));p.drawPath(wave);p.setPen(Qt::NoPen);}}
        break;
    case NativePreset::Silk:
        if(!ornament){gradient({20,15,460,h-35},QColor("#efc7d8"),QColor("#cc9bb9"),42);path(polygon({{270,15},{323,15},{239,h-20},{194,h-20}}),QColor(255,232,238,140));}
        else {QLinearGradient sheen(225,0,300,0);sheen.setColorAt(0,QColor(255,255,255,0));sheen.setColorAt(.5,QColor(255,241,245,92));sheen.setColorAt(1,QColor(255,255,255,0));p.setBrush(sheen);p.drawRect(QRectF(225,22,75,h-49));}
        break;
    case NativePreset::Obsidian:
        if(!ornament){path(polygon({{22,35},{43,15},{450,15},{479,45},{479,h-43},{445,h-20},{22,h-20}}),QColor("#262331"));path(polygon({{22,35},{43,15},{260,15},{190,ty+th},{22,h-20}}),QColor("#30323e"));}
        else {path(polygon({{393,cy-48},{448,cy-30},{457,cy+28},{420,cy+50},{383,cy+25}}),QColor("#46384f"));path(polygon({{393,cy-48},{411,cy-25},{387,cy+22},{383,cy+25}}),QColor("#837092"));}
        break;
    case NativePreset::Lens:
        if(!ornament){for(const auto point:{QPointF(42,48),QPointF(458,48),QPointF(42,h-45),QPointF(458,h-45)}){const double x=point.x(),y=point.y(),dx=x<100?19:-19,dy=y<100?18:-18;line({x,y},{x+dx,y},QColor("#e6ffff"),1.5);line({x,y},{x,y+dy},QColor("#e6ffff"),1.5);}disc({45,25},3,QColor("#ff6d64"));for(int i=0;i<20;++i)rect({280.0+i*6,h-29,1,i%3?5.0:9.0},QColor("#8aa0ab"));}
        else {line({401,cy-9},{401,cy+9},QColor("#6adbd3"));line({392,cy},{410,cy},QColor("#6adbd3"));}
        break;
    case NativePreset::Bubblegum:
        if(!ornament){rect({185,18,130,25},QColor("#a8e9ff"),12);path(polygon({{40,ty+th+8},{455,ty+th+8},{463,ty+th+13},{44,ty+th+15}}),QColor("#7c326c"));}
        else {QRadialGradient g(QPointF(65,35),35);g.setColorAt(0,QColor("#edfeff"));g.setColorAt(.45,QColor("#77ccff"));g.setColorAt(1,QColor("#3863d0"));p.setBrush(g);p.drawEllipse(QPointF(78,48),26,26);ring({430,48},17,QColor("#c698ff"),9);disc({460,h-40},11,QColor("#79cfff"));}
        break;
    case NativePreset::Tuning:
        if(!ornament){line({43,19},{459,19},QColor("#526332"));line({43,h-29},{459,h-29},QColor("#647a3d"));for(int i=0;i<43;++i)line({43.0+i*9.5,h-41},{43.0+i*9.5,h-29},QColor("#647a3d"));}
        else {rect({300,h-44,3,16},QColor("#dfff8e"));for(int i=0;i<10;++i)rect({43.0+i*6,h-16,2,4+(i%4)*3.0},QColor("#dfff8e"));}
        break;
    case NativePreset::Trace:
        if(!ornament){p.setBrush(Qt::NoBrush);p.setPen(QPen(QColor("#dc7bbb"),1));p.drawRoundedRect(QRectF(48,20,430,h-55),30,30);p.setPen(Qt::NoPen);line({25,17},{25,h-18},QColor("#626376"));for(int i=0;i<7;++i)line({20,30+i*15.0},{26,30+i*15.0},QColor("#928197"));}
        else {disc({49,h-35},3,QColor("#ff92d5"));rect({402,20,35,2},QColor("#ff92d5"));}
        break;
    default:throw std::invalid_argument("Missing native NORMAL art recipe");
    }
}
}
