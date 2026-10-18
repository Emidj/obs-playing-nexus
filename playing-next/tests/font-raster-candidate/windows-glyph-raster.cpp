// SPDX-License-Identifier: GPL-2.0-or-later
#define NOMINMAX
#include "playing/visual/windows-glyph-raster.hpp"
#include <dwrite_3.h>
#include <d2d1.h>
#include <wrl/client.h>
#include <QPainter>
#include <QPainterPath>
#include <QGuiApplication>
#include <QThread>
#include <stdexcept>
#include <vector>
#include <iostream>
#include <array>
#include <cmath>
namespace playing::visual::normal {
using Microsoft::WRL::ComPtr;
#ifdef PLAYING_SKIA_PATH_DIAGNOSTIC
QImage rasterSkiaColorPath(const QPainterPath &,QSize,QColor,QPointF,const QString &);
QImage rasterSkiaColorGlyph(const QString &,int,float,std::uint16_t,QSize,QColor,QPointF,const QString &);
#endif
namespace {
void require(HRESULT result){if(FAILED(result))throw std::runtime_error("DirectWrite glyph preparation failed");}
class OutlineSink final:public IDWriteGeometrySink {
    ULONG references_=1;
public:
    QPainterPath path;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void **result)override{
        if(!result)return E_POINTER;*result=nullptr;
        if(id==__uuidof(IUnknown)||id==__uuidof(IDWriteGeometrySink)){*result=static_cast<IDWriteGeometrySink*>(this);AddRef();return S_OK;}return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef()override{return ++references_;}
    ULONG STDMETHODCALLTYPE Release()override{const auto left=--references_;if(!left)delete this;return left;}
    void STDMETHODCALLTYPE SetFillMode(D2D1_FILL_MODE mode)override{path.setFillRule(mode==D2D1_FILL_MODE_WINDING?Qt::WindingFill:Qt::OddEvenFill);}
    void STDMETHODCALLTYPE SetSegmentFlags(D2D1_PATH_SEGMENT)override{}
    void STDMETHODCALLTYPE BeginFigure(D2D1_POINT_2F point,D2D1_FIGURE_BEGIN)override{path.moveTo(point.x,point.y);}
    void STDMETHODCALLTYPE AddLines(const D2D1_POINT_2F *points,UINT32 count)override{for(UINT32 i=0;i<count;++i)path.lineTo(points[i].x,points[i].y);}
    void STDMETHODCALLTYPE AddBeziers(const D2D1_BEZIER_SEGMENT *curves,UINT32 count)override{for(UINT32 i=0;i<count;++i){const auto &c=curves[i];path.cubicTo(c.point1.x,c.point1.y,c.point2.x,c.point2.y,c.point3.x,c.point3.y);}}
    void STDMETHODCALLTYPE EndFigure(D2D1_FIGURE_END end)override{if(end==D2D1_FIGURE_END_CLOSED)path.closeSubpath();}
    HRESULT STDMETHODCALLTYPE Close()override{return S_OK;}
};
void paintColorOutline(QPainter &painter,const DWRITE_GLYPH_RUN &run,float x,float y,QColor color,QSize size,const QString &library){
    ComPtr<OutlineSink> sink;sink.Attach(new OutlineSink);
    require(run.fontFace->GetGlyphRunOutline(run.fontEmSize,run.glyphIndices,run.glyphAdvances,run.glyphOffsets,run.glyphCount,run.isSideways,run.bidiLevel%2,sink.Get()));
    const QPointF origin(std::floor(x*4+0.5f)/4,std::round(y));
    if(!library.isEmpty()){
#ifdef PLAYING_SKIA_PATH_DIAGNOSTIC
        painter.drawImage(0,0,rasterSkiaColorPath(sink->path,size,color,origin,library));return;
#else
        Q_UNUSED(size);throw std::invalid_argument("Path diagnostic not enabled");
#endif
    }
    painter.save();painter.translate(origin);painter.setRenderHint(QPainter::Antialiasing);painter.setPen(Qt::NoPen);painter.setBrush(color);painter.drawPath(sink->path);painter.restore();
}
// Diagnostic implementation of the luminance/preblend model documented by
// Skia 127. Parameters come from Windows, never from reference pixels.
std::array<int,256> systemCoverage(IDWriteFactory3 *factory,QColor color){
    ComPtr<IDWriteRenderingParams> parameters;require(factory->CreateRenderingParams(&parameters));
    const double gamma=std::floor(parameters->GetGamma()*64)/64;
    const double contrast=std::round(std::min(1.0f,parameters->GetEnhancedContrast())*255)/255;
    const auto canonical=[](int value){const int n=value>>5;return (n<<5)|(n<<2)|(n>>1);};
    const int luminance=(canonical(color.red())*54+canonical(color.green())*183+canonical(color.blue())*19)>>8;
    const double source=canonical(luminance)/255.0,destination=1-source;
    std::array<int,256> values{};
    for(int i=0;i<256;++i){double a=i/255.0;a+=a*(1-a)*contrast*std::pow(destination,gamma);
        double mapped=a;if(std::abs(source-destination)>=1.0/256)
            mapped=(std::pow(std::pow(source,gamma)*a+std::pow(destination,gamma)*(1-a),1/gamma)-destination)/(source-destination);
        values[i]=std::clamp(static_cast<int>(std::round(mapped*255)),0,255);}
    return values;
}
DWRITE_RENDERING_MODE renderingFor(const DWRITE_GLYPH_RUN &run){
    const void *bytes=nullptr;UINT32 length=0;void *context=nullptr;BOOL exists=FALSE;
    require(run.fontFace->TryGetFontTable(DWRITE_MAKE_OPENTYPE_TAG('g','a','s','p'),&bytes,&length,&context,&exists));
    auto selected=run.fontEmSize>20?DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC:DWRITE_RENDERING_MODE_NATURAL;
    if(exists&&length>=4){const auto *data=static_cast<const BYTE*>(bytes);
        const auto word=[&](UINT32 at){return (static_cast<unsigned>(data[at])<<8)|data[at+1];};
        const unsigned ranges=word(2);if(word(0)>=1&&ranges<=(length-4)/4)
            for(unsigned i=0;i<ranges;++i){const UINT32 at=4+i*4;if(word(at)>=std::round(run.fontEmSize)){
                selected=(word(at+2)&8)?DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC:DWRITE_RENDERING_MODE_NATURAL;break;}}}
    static bool first=true;if(first){std::cout<<"gasp exists="<<exists<<" bytes="<<length<<" size="<<run.fontEmSize<<" mode="<<selected<<'\n';first=false;}
    if(context)run.fontFace->ReleaseFontTable(context);
    return selected;
}
void paint(IDWriteFactory3 *factory,QPainter &painter,const DWRITE_GLYPH_RUN &run,
           float x,float y,QColor color,const QRect &clip,bool downsample,bool natural,bool gray,bool systemPreblend){
    ComPtr<IDWriteGlyphRunAnalysis> analysis;
    const auto mode=natural?DWRITE_RENDERING_MODE1_NATURAL:downsample?DWRITE_RENDERING_MODE1_NATURAL_SYMMETRIC_DOWNSAMPLED:DWRITE_RENDERING_MODE1_NATURAL_SYMMETRIC;
    if(systemPreblend){const float advance=0;const DWRITE_GLYPH_OFFSET offset{};auto placed=run;
        placed.glyphAdvances=&advance;placed.glyphOffsets=&offset;
        const DWRITE_MATRIX transform{1,0,0,1,std::floor(x*4+0.5f)/4,std::round(y)};
        require(static_cast<IDWriteFactory*>(factory)->CreateGlyphRunAnalysis(&placed,1,&transform,
            renderingFor(run),DWRITE_MEASURING_MODE_NATURAL,0,0,&analysis));
    }else require(factory->CreateGlyphRunAnalysis(&run,nullptr,mode,
        DWRITE_MEASURING_MODE_NATURAL,DWRITE_GRID_FIT_MODE_ENABLED,(downsample||gray)?DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE:DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE,x,y,&analysis));
    const auto texture=(downsample||gray)?DWRITE_TEXTURE_ALIASED_1x1:DWRITE_TEXTURE_CLEARTYPE_3x1;
    const int channels=(downsample||gray)?1:3;
    RECT bounds{};require(analysis->GetAlphaTextureBounds(texture,&bounds));
    const auto rectangle=QRect(bounds.left,bounds.top,bounds.right-bounds.left,bounds.bottom-bounds.top).intersected(clip);
    if(rectangle.isEmpty())return;
    bounds={rectangle.left(),rectangle.top(),rectangle.right()+1,rectangle.bottom()+1};
    std::vector<BYTE> mask(static_cast<std::size_t>(rectangle.width())*rectangle.height()*channels);
    require(analysis->CreateAlphaTexture(texture,&bounds,mask.data(),static_cast<UINT32>(mask.size())));
    QImage pixels(rectangle.size(),QImage::Format_RGBA8888_Premultiplied);
    const auto table=systemPreblend?systemCoverage(factory,color):std::array<int,256>{};
    for(int row=0;row<rectangle.height();++row){auto *target=pixels.scanLine(row);
        for(int column=0;column<rectangle.width();++column){const auto offset=(static_cast<std::size_t>(row)*rectangle.width()+column)*channels;
            const int raw=channels==1?mask[offset]:(mask[offset]+mask[offset+1]+mask[offset+2]+(systemPreblend?0:1))/3;
            const int coverage=systemPreblend?table[raw]:raw;
            const int alpha=(coverage*color.alpha()+127)/255;
            target[column*4]=static_cast<uchar>((color.red()*alpha+127)/255);target[column*4+1]=static_cast<uchar>((color.green()*alpha+127)/255);
            target[column*4+2]=static_cast<uchar>((color.blue()*alpha+127)/255);target[column*4+3]=static_cast<uchar>(alpha);}}
    painter.drawImage(rectangle.topLeft(),pixels);
}
}
QImage rasterWindowsGlyphs(const QList<QGlyphRun> &runs,QSize size,QColor color,QPointF origin,bool downsample,bool natural,bool gray,bool systemPreblend,const QString &colorPathLibrary,bool colorGlyph){
    if(!qobject_cast<QGuiApplication*>(QCoreApplication::instance())||QThread::currentThread()!=qApp->thread())
        throw std::logic_error("DirectWrite candidate requires GUI owner");
    if(size.width()<1||size.width()>2048||size.height()<1||size.height()>256||!color.isValid())
        throw std::invalid_argument("DirectWrite glyph raster bounds");
    ComPtr<IDWriteFactory3> factory;require(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory3),reinterpret_cast<IUnknown**>(factory.GetAddressOf())));
    static bool reported=false;if(!reported){ComPtr<IDWriteRenderingParams> parameters;require(factory->CreateRenderingParams(&parameters));
        std::cout<<"DirectWrite system gamma="<<parameters->GetGamma()<<" contrast="<<parameters->GetEnhancedContrast()<<'\n';reported=true;}
    ComPtr<IDWriteFontCollection> collection;require(factory->GetSystemFontCollection(&collection));
    QImage result(size,QImage::Format_RGBA8888_Premultiplied);result.fill(Qt::transparent);QPainter painter(&result);
    const QRect clip(QPoint{},size);
    for(const auto &qtRun:runs){const auto raw=qtRun.rawFont();UINT32 index=0;BOOL exists=FALSE;
        const auto familyName=raw.familyName().toStdWString();require(collection->FindFamilyName(familyName.c_str(),&index,&exists));
        if(!exists)throw std::runtime_error("DirectWrite glyph font family unavailable");
        ComPtr<IDWriteFontFamily> family;require(collection->GetFontFamily(index,&family));ComPtr<IDWriteFont> font;
        require(family->GetFirstMatchingFont(static_cast<DWRITE_FONT_WEIGHT>(raw.weight()),DWRITE_FONT_STRETCH_NORMAL,raw.style()==QFont::StyleItalic?DWRITE_FONT_STYLE_ITALIC:DWRITE_FONT_STYLE_NORMAL,&font));
        ComPtr<IDWriteFontFace> face;require(font->CreateFontFace(&face));const auto indexes=qtRun.glyphIndexes();const auto positions=qtRun.positions();
        if(indexes.size()!=positions.size())throw std::invalid_argument("DirectWrite glyph positions differ");
        for(qsizetype i=0;i<indexes.size();++i){if(indexes[i]>65535)throw std::invalid_argument("DirectWrite glyph index bounds");
            const UINT16 glyph=static_cast<UINT16>(indexes[i]);DWRITE_GLYPH_RUN run{};run.fontFace=face.Get();run.fontEmSize=static_cast<float>(raw.pixelSize());run.glyphCount=1;run.glyphIndices=&glyph;
            const auto position=positions[i]+origin;const float x=static_cast<float>(position.x()),y=static_cast<float>(position.y());
            ComPtr<IDWriteColorGlyphRunEnumerator> layers;const HRESULT colored=factory->TranslateColorGlyphRun(x,y,&run,nullptr,DWRITE_MEASURING_MODE_NATURAL,nullptr,0,&layers);
            if(colored==DWRITE_E_NOCOLOR){paint(factory.Get(),painter,run,x,y,color,clip,downsample,natural,gray,systemPreblend);continue;}require(colored);
            if(colorGlyph){
#ifdef PLAYING_SKIA_PATH_DIAGNOSTIC
                painter.drawImage(0,0,rasterSkiaColorGlyph(raw.familyName(),static_cast<int>(raw.weight()),run.fontEmSize,glyph,size,color,{x,y},colorPathLibrary));continue;
#else
                throw std::invalid_argument("Color glyph diagnostic not enabled");
#endif
            }
            BOOL more=FALSE;while(true){require(layers->MoveNext(&more));if(!more)break;const DWRITE_COLOR_GLYPH_RUN *layer=nullptr;require(layers->GetCurrentRun(&layer));
                const QColor fill=layer->paletteIndex==0xffff?color:QColor::fromRgbF(layer->runColor.r,layer->runColor.g,layer->runColor.b,layer->runColor.a);
                if(systemPreblend)paintColorOutline(painter,layer->glyphRun,layer->baselineOriginX,layer->baselineOriginY,fill,size,colorPathLibrary);
                else paint(factory.Get(),painter,layer->glyphRun,layer->baselineOriginX,layer->baselineOriginY,fill,clip,downsample,natural,gray,systemPreblend);}
        }
    }
    return result;
}
}
