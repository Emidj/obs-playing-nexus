// SPDX-License-Identifier: MIT
#pragma once
#include <QImage>
#include <QPainter>
#include <QTextLayout>
#include <QFontMetricsF>
#include <stdexcept>
#include <algorithm>
namespace playing::visual {
struct TextEffects {
    int outline=0,shadow=0;
    bool underline=false,strikeout=false;
    QColor outlineColor=Qt::black,shadowColor=QColor(0,0,0,160);
    int padding()const{return std::max(outline,shadow);}
    void validate()const{if(outline<0||outline>8||shadow<0||shadow>8||!outlineColor.isValid()||!shadowColor.isValid())throw std::invalid_argument("Invalid text effects");}
    bool operator==(const TextEffects& other)const{return outline==other.outline&&shadow==other.shadow&&underline==other.underline&&strikeout==other.strikeout&&outlineColor==other.outlineColor&&shadowColor==other.shadowColor;}
};
inline void drawTextDecorations(QPainter &painter,const QTextLayout &layout,const TextEffects &effect,QColor color,QPointF origin={}){
    if(!effect.underline&&!effect.strikeout)return;
    const QFontMetricsF metrics(layout.font());const qreal scale=std::max(.001,std::abs(painter.transform().m11()));const qreal thickness=std::max(1.0/scale,metrics.lineWidth());
    for(int i=0;i<layout.lineCount();++i){const auto line=layout.lineAt(i);const auto a=line.cursorToX(line.textStart()),b=line.cursorToX(line.textStart()+line.textLength());const qreal left=std::min(a,b),width=std::abs(a-b),baseline=line.y()+line.ascent();
        if(effect.underline)painter.fillRect(QRectF(origin.x()+left,origin.y()+baseline+metrics.underlinePos(),width,thickness),color);
        if(effect.strikeout)painter.fillRect(QRectF(origin.x()+left,origin.y()+baseline-metrics.strikeOutPos(),width,thickness),color);
    }
}
// GUI preparation only, premultiplied masks; zero strength is exact identity.
inline QImage applyTextEffects(QImage image,const TextEffects &effect){
    effect.validate();if(!effect.padding())return image;
    image=image.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
    const int w=image.width(),h=image.height();
    if(w>2048||h>2048)throw std::invalid_argument("Text effect raster exceeds tile budget");
    QImage result(image.size(),image.format());result.fill(Qt::transparent);
    QImage dilated=image;
    if(effect.outline){QImage horizontal(image.size(),image.format());horizontal.fill(Qt::transparent);dilated=horizontal.copy();
        for(int y=0;y<h;++y)for(int x=0;x<w;++x){int alpha=0;for(int dx=-effect.outline;dx<=effect.outline;++dx)if(x+dx>=0&&x+dx<w)alpha=std::max(alpha,int(image.constScanLine(y)[(x+dx)*4+3]));horizontal.scanLine(y)[x*4+3]=uchar(alpha);}
        for(int y=0;y<h;++y)for(int x=0;x<w;++x){int alpha=0;for(int dy=-effect.outline;dy<=effect.outline;++dy)if(y+dy>=0&&y+dy<h)alpha=std::max(alpha,int(horizontal.constScanLine(y+dy)[x*4+3]));dilated.scanLine(y)[x*4+3]=uchar(alpha);}
    }
    const auto tint=[&](bool outline){QImage mask(image.size(),image.format());mask.fill(Qt::transparent);const auto color=outline?effect.outlineColor:effect.shadowColor;
        for(int y=0;y<h;++y)for(int x=0;x<w;++x){int alpha=0;
            if(outline)alpha=dilated.constScanLine(y)[x*4+3];
            else{const int sx=x-effect.shadow,sy=y-effect.shadow;if(sx>=0&&sy>=0)alpha=image.constScanLine(sy)[sx*4+3];}
            alpha=alpha*color.alpha()/255;auto *pixel=mask.scanLine(y)+x*4;pixel[0]=uchar(color.red()*alpha/255);pixel[1]=uchar(color.green()*alpha/255);pixel[2]=uchar(color.blue()*alpha/255);pixel[3]=uchar(alpha);
        }return mask;};
    QPainter painter(&result);if(effect.shadow)painter.drawImage(0,0,tint(false));if(effect.outline)painter.drawImage(0,0,tint(true));painter.drawImage(0,0,image);painter.end();return result;
}
inline QImage paddedTextEffects(const QImage &image,const TextEffects &effect){
    effect.validate();if(!effect.padding())return image;const int pad=effect.padding();
    QImage padded(image.size()+QSize(pad*2,pad*2),QImage::Format_RGBA8888_Premultiplied);padded.fill(Qt::transparent);{QPainter painter(&padded);painter.drawImage(pad,pad,image);}return applyTextEffects(std::move(padded),effect);
}
}
