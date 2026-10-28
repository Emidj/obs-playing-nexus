// SPDX-License-Identifier: GPL-2.0-or-later
#define NOMINMAX
#include "playing/visual/native-glyph-raster.hpp"
#include <dwrite_3.h>
#include <wrl/client.h>
#include <QGuiApplication>
#include <QThread>
#include <QPainter>
#include <QFile>
#include <QFileInfo>
#include <QLibrary>
#include <QCryptographicHash>
#include <QScopeGuard>
#include <array>
#include <list>
#include <cmath>
#include <cstring>
#include "include/c/sk_font.h"
#include "include/c/sk_typeface.h"
#include "include/c/sk_surface.h"
#include "include/c/sk_canvas.h"
#include "include/c/sk_paint.h"
#include "include/c/sk_textblob.h"
#include "include/c/sk_image.h"
#include "include/c/sk_imagefilter.h"
namespace playing::visual::normal {
using Microsoft::WRL::ComPtr;
namespace {
void owner(){
    if(!qobject_cast<QGuiApplication*>(QCoreApplication::instance())||QThread::currentThread()!=qApp->thread())
        throw std::logic_error("Native glyph preparation requires GUI owner");
}
void require(HRESULT status){if(FAILED(status))throw std::runtime_error("Native DirectWrite preparation failed");}
// Public native C API. The manager/library live together; no managed runtime.
class ColorRaster final {
    QLibrary library_;
    sk_fontmgr_t *manager_=nullptr;
#define DECLARE(name) decltype(&::name) name=nullptr
    DECLARE(sk_fontmgr_create_default);DECLARE(sk_fontmgr_unref);DECLARE(sk_fontmgr_match_family_style);
    DECLARE(sk_fontstyle_new);DECLARE(sk_fontstyle_delete);DECLARE(sk_typeface_unref);
    DECLARE(sk_font_new_with_values);DECLARE(sk_font_delete);DECLARE(sk_font_set_hinting);DECLARE(sk_font_set_edging);
    DECLARE(sk_font_set_subpixel);DECLARE(sk_font_set_linear_metrics);DECLARE(sk_font_set_baseline_snap);
    DECLARE(sk_surface_new_raster_direct);DECLARE(sk_surface_get_canvas);DECLARE(sk_surface_unref);
    DECLARE(sk_textblob_builder_new);DECLARE(sk_textblob_builder_delete);DECLARE(sk_textblob_builder_alloc_run_pos);
    DECLARE(sk_textblob_builder_make);DECLARE(sk_textblob_unref);
    DECLARE(sk_paint_new);DECLARE(sk_paint_delete);DECLARE(sk_paint_set_color);DECLARE(sk_paint_set_antialias);DECLARE(sk_canvas_draw_text_blob);
    DECLARE(sk_image_new_raster_copy);DECLARE(sk_image_unref);DECLARE(sk_imagefilter_new_blur);DECLARE(sk_imagefilter_unref);
    DECLARE(sk_paint_set_imagefilter);DECLARE(sk_canvas_draw_image);
#undef DECLARE
    struct Face {QString family;int weight;sk_typeface_t *face;};
    std::list<Face> faces_;
public:
    explicit ColorRaster(const QString &path):library_(path){
        if(!QFileInfo(path).isAbsolute())throw std::invalid_argument("Native color library requires absolute path");
        QFile file(path);if(!file.open(QIODevice::ReadOnly))throw std::runtime_error("Native color library unavailable");
        QCryptographicHash hash(QCryptographicHash::Sha256);
        if(!hash.addData(&file)||hash.result().toHex()!=QByteArrayLiteral("5fffeed011731a3255022f003045995f5e96810ca5538c5fe4a91b048b32560e"))
            throw std::runtime_error("Native color library hash mismatch");
        file.close();if(!library_.load())throw std::runtime_error("Native color library load failed");
        auto loadGuard=qScopeGuard([&]{library_.unload();});
#define LOAD(name) name=reinterpret_cast<decltype(name)>(library_.resolve(#name));if(!name)throw std::runtime_error(#name)
        LOAD(sk_fontmgr_create_default);LOAD(sk_fontmgr_unref);LOAD(sk_fontmgr_match_family_style);
        LOAD(sk_fontstyle_new);LOAD(sk_fontstyle_delete);LOAD(sk_typeface_unref);
        LOAD(sk_font_new_with_values);LOAD(sk_font_delete);LOAD(sk_font_set_hinting);LOAD(sk_font_set_edging);
        LOAD(sk_font_set_subpixel);LOAD(sk_font_set_linear_metrics);LOAD(sk_font_set_baseline_snap);
        LOAD(sk_surface_new_raster_direct);LOAD(sk_surface_get_canvas);LOAD(sk_surface_unref);
        LOAD(sk_textblob_builder_new);LOAD(sk_textblob_builder_delete);LOAD(sk_textblob_builder_alloc_run_pos);
        LOAD(sk_textblob_builder_make);LOAD(sk_textblob_unref);
        LOAD(sk_paint_new);LOAD(sk_paint_delete);LOAD(sk_paint_set_color);LOAD(sk_paint_set_antialias);LOAD(sk_canvas_draw_text_blob);
        LOAD(sk_image_new_raster_copy);LOAD(sk_image_unref);LOAD(sk_imagefilter_new_blur);LOAD(sk_imagefilter_unref);
        LOAD(sk_paint_set_imagefilter);LOAD(sk_canvas_draw_image);
#undef LOAD
        manager_=sk_fontmgr_create_default();if(!manager_)throw std::runtime_error("Native color manager unavailable");
        loadGuard.dismiss();
    }
    ~ColorRaster(){for(const auto &face:faces_)sk_typeface_unref(face.face);if(manager_)sk_fontmgr_unref(manager_);library_.unload();}
    QImage blur(const QImage &pixels,double radius){
        QImage output(pixels.size(),QImage::Format_RGBA8888_Premultiplied);output.fill(Qt::transparent);
        const sk_imageinfo_t info{nullptr,pixels.width(),pixels.height(),RGBA_8888_SK_COLORTYPE,PREMUL_SK_ALPHATYPE};
        auto image=sk_image_new_raster_copy(&info,pixels.constBits(),pixels.bytesPerLine());
        if(!image)throw std::runtime_error("Native shadow image unavailable");const auto imageGuard=qScopeGuard([&]{sk_image_unref(image);});
        auto filter=sk_imagefilter_new_blur(static_cast<float>(radius*.5),static_cast<float>(radius*.5),DECAL_SK_SHADER_TILEMODE,nullptr,nullptr);
        if(!filter)throw std::runtime_error("Native shadow filter unavailable");const auto filterGuard=qScopeGuard([&]{sk_imagefilter_unref(filter);});
        auto surface=sk_surface_new_raster_direct(&info,output.bits(),output.bytesPerLine(),nullptr,nullptr,nullptr);
        if(!surface)throw std::runtime_error("Native shadow surface unavailable");const auto surfaceGuard=qScopeGuard([&]{sk_surface_unref(surface);});
        auto paint=sk_paint_new();if(!paint)throw std::runtime_error("Native shadow paint unavailable");const auto paintGuard=qScopeGuard([&]{sk_paint_delete(paint);});
        sk_paint_set_imagefilter(paint,filter);const sk_sampling_options_t sampling{};
        sk_canvas_draw_image(sk_surface_get_canvas(surface),image,0,0,&sampling,paint);return output;
    }
    QImage raster(const QRawFont &raw,std::uint16_t glyph,QSize size,QColor color,QPointF origin){
        sk_typeface_t *face=nullptr;
        for(auto it=faces_.begin();it!=faces_.end();++it)if(it->family==raw.familyName()&&it->weight==raw.weight()){
            face=it->face;faces_.splice(faces_.begin(),faces_,it);break;
        }
        if(!face){
            auto style=sk_fontstyle_new(raw.weight(),5,UPRIGHT_SK_FONT_STYLE_SLANT);
            if(!style)throw std::runtime_error("Native color style unavailable");
            const auto styleGuard=qScopeGuard([&]{sk_fontstyle_delete(style);});
            face=sk_fontmgr_match_family_style(manager_,raw.familyName().toUtf8().constData(),style);
            if(!face)throw std::runtime_error("Native color face unavailable");
            auto faceGuard=qScopeGuard([&]{sk_typeface_unref(face);});
            faces_.push_front({raw.familyName(),raw.weight(),face});faceGuard.dismiss();
            if(faces_.size()>32){sk_typeface_unref(faces_.back().face);faces_.pop_back();}
        }
        auto font=sk_font_new_with_values(face,static_cast<float>(raw.pixelSize()),1,0);
        if(!font)throw std::runtime_error("Native color font unavailable");const auto fontGuard=qScopeGuard([&]{sk_font_delete(font);});
        sk_font_set_hinting(font,NORMAL_SK_FONT_HINTING);sk_font_set_edging(font,ANTIALIAS_SK_FONT_EDGING);
        sk_font_set_subpixel(font,true);sk_font_set_linear_metrics(font,true);sk_font_set_baseline_snap(font,false);
        QImage pixels(size,QImage::Format_RGBA8888_Premultiplied);pixels.fill(Qt::transparent);
        sk_imageinfo_t info{nullptr,size.width(),size.height(),RGBA_8888_SK_COLORTYPE,PREMUL_SK_ALPHATYPE};
        auto surface=sk_surface_new_raster_direct(&info,pixels.bits(),pixels.bytesPerLine(),nullptr,nullptr,nullptr);
        if(!surface)throw std::runtime_error("Native color surface unavailable");const auto surfaceGuard=qScopeGuard([&]{sk_surface_unref(surface);});
        auto paint=sk_paint_new();if(!paint)throw std::runtime_error("Native color paint unavailable");const auto paintGuard=qScopeGuard([&]{sk_paint_delete(paint);});
        sk_paint_set_color(paint,color.rgba());sk_paint_set_antialias(paint,true);
        auto builder=sk_textblob_builder_new();if(!builder)throw std::runtime_error("Native color builder unavailable");
        const auto builderGuard=qScopeGuard([&]{sk_textblob_builder_delete(builder);});
        sk_textblob_builder_runbuffer_t buffer{};sk_textblob_builder_alloc_run_pos(builder,font,1,nullptr,&buffer);
        if(!buffer.glyphs||!buffer.pos)throw std::runtime_error("Native color buffer unavailable");
        *static_cast<std::uint16_t*>(buffer.glyphs)=glyph;auto points=static_cast<float*>(buffer.pos);
        points[0]=static_cast<float>(origin.x());points[1]=static_cast<float>(origin.y());
        auto blob=sk_textblob_builder_make(builder);if(!blob)throw std::runtime_error("Native color blob unavailable");
        const auto blobGuard=qScopeGuard([&]{sk_textblob_unref(blob);});
        sk_canvas_draw_text_blob(sk_surface_get_canvas(surface),blob,0,0,paint);return pixels;
    }
};
DWRITE_RENDERING_MODE modeFor(const DWRITE_GLYPH_RUN &run){
    const void *bytes=nullptr;UINT32 length=0;void *context=nullptr;BOOL exists=FALSE;
    require(run.fontFace->TryGetFontTable(DWRITE_MAKE_OPENTYPE_TAG('g','a','s','p'),&bytes,&length,&context,&exists));
    const auto tableGuard=qScopeGuard([&]{if(context)run.fontFace->ReleaseFontTable(context);});
    auto mode=run.fontEmSize>20?DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC:DWRITE_RENDERING_MODE_NATURAL;
    if(exists&&length>=4){const auto *data=static_cast<const BYTE*>(bytes);
        const auto word=[&](UINT32 at){return (static_cast<unsigned>(data[at])<<8)|data[at+1];};
        const unsigned ranges=word(2);if(word(0)>=1&&ranges<=(length-4)/4)
            for(unsigned i=0;i<ranges;++i){const UINT32 at=4+i*4;if(word(at)>=std::round(run.fontEmSize)){
                mode=(word(at+2)&8)?DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC:DWRITE_RENDERING_MODE_NATURAL;break;}}
    }
    return mode;
}
}
struct NativeGlyphRasterizer::Impl {
    QThread *thread=QThread::currentThread();
    ComPtr<IDWriteFactory3> factory;
    ComPtr<IDWriteFontCollection> collection;
    QString binary;
    std::unique_ptr<ColorRaster> colors;
    double gamma=0,contrast=0;
    struct Face {QString family;int weight;QFont::Style style;QString styleName;ComPtr<IDWriteFontFace> face;};
    std::list<Face> faces;
    explicit Impl(QString path):binary(std::move(path)){
        require(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory3),reinterpret_cast<IUnknown**>(factory.GetAddressOf())));
        require(factory->GetSystemFontCollection(&collection));ComPtr<IDWriteRenderingParams> parameters;
        require(factory->CreateRenderingParams(&parameters));gamma=std::floor(parameters->GetGamma()*64)/64;
        contrast=std::round(std::min(1.0f,parameters->GetEnhancedContrast())*255)/255;
    }
    IDWriteFontFace *faceFor(const QRawFont &raw){
        for(auto it=faces.begin();it!=faces.end();++it)if(it->family==raw.familyName()&&it->weight==raw.weight()&&it->style==raw.style()&&it->styleName==raw.styleName()){
            auto *result=it->face.Get();faces.splice(faces.begin(),faces,it);return result;
        }
        UINT32 index=0;BOOL exists=FALSE;const auto name=raw.familyName().toStdWString();
        require(collection->FindFamilyName(name.c_str(),&index,&exists));ComPtr<IDWriteFont> font;
        if(exists){
            ComPtr<IDWriteFontFamily> family;require(collection->GetFontFamily(index,&family));
            // Some Qt runtimes report normal weight for a named physical face.
            // Prefer its actual face name instead of rematching only weight.
            if(family->GetFontCount()>128)throw std::runtime_error("Native font family exceeds matching bounds");
            for(UINT32 candidate=0;candidate<family->GetFontCount()&&!font;++candidate){
                ComPtr<IDWriteFont> possible;require(family->GetFont(candidate,&possible));
                ComPtr<IDWriteLocalizedStrings> names;require(possible->GetFaceNames(&names));
                if(names->GetCount()>128)throw std::runtime_error("Native font names exceed matching bounds");
                for(UINT32 locale=0;locale<names->GetCount();++locale){UINT32 length=0;require(names->GetStringLength(locale,&length));
                    if(length>128)continue;std::vector<wchar_t> text(length+1);require(names->GetString(locale,text.data(),length+1));
                    if(QString::fromWCharArray(text.data()).compare(raw.styleName(),Qt::CaseInsensitive)==0){font=possible;break;}
                }
            }
            if(!font)require(family->GetFirstMatchingFont(static_cast<DWRITE_FONT_WEIGHT>(raw.weight()),DWRITE_FONT_STRETCH_NORMAL,
                raw.style()==QFont::StyleItalic?DWRITE_FONT_STYLE_ITALIC:DWRITE_FONT_STYLE_NORMAL,&font));
        }else{
            // Qt can expose a Win32 family (Arial Black) absent from the WWS
            // collection. Resolve that model through official DirectWrite GDI
            // interoperability, then verify actual font tables before caching.
            if(name.size()>=LF_FACESIZE)throw std::runtime_error("Native glyph legacy family exceeds LOGFONT bounds");
            LOGFONTW legacy{};std::copy(name.begin(),name.end(),legacy.lfFaceName);
            legacy.lfWeight=raw.weight();legacy.lfItalic=raw.style()!=QFont::StyleNormal;
            legacy.lfCharSet=DEFAULT_CHARSET;
            ComPtr<IDWriteGdiInterop> interop;require(factory->GetGdiInterop(&interop));
            require(interop->CreateFontFromLOGFONT(&legacy,&font));
        }
        ComPtr<IDWriteFontFace> face;require(font->CreateFontFace(&face));
        if(!exists){
            for(const char *tag:{"head","maxp","cmap"}){
                const auto expected=raw.fontTable(tag);const void *data=nullptr;UINT32 length=0;void *context=nullptr;BOOL found=FALSE;
                require(face->TryGetFontTable(DWRITE_MAKE_OPENTYPE_TAG(tag[0],tag[1],tag[2],tag[3]),&data,&length,&context,&found));
                auto release=qScopeGuard([&]{if(found)face->ReleaseFontTable(context);});
                if(!found||expected.isEmpty()||length!=static_cast<UINT32>(expected.size())||
                    std::memcmp(data,expected.constData(),length)!=0)
                    throw std::runtime_error("Native legacy family resolved to a different font");
            }
        }
        faces.push_front({raw.familyName(),raw.weight(),raw.style(),raw.styleName(),face});
        if(faces.size()>32)faces.pop_back();return faces.front().face.Get();
    }
    std::array<int,256> coverage(QColor color)const{
        const auto canonical=[](int value){const int n=value>>5;return (n<<5)|(n<<2)|(n>>1);};
        const int luminance=(canonical(color.red())*54+canonical(color.green())*183+canonical(color.blue())*19)>>8;
        const double source=canonical(luminance)/255.0,destination=1-source;std::array<int,256> values{};
        for(int i=0;i<256;++i){double a=i/255.0;a+=a*(1-a)*contrast*std::pow(destination,gamma);
            double mapped=a;if(std::abs(source-destination)>=1.0/256)
                mapped=(std::pow(std::pow(source,gamma)*a+std::pow(destination,gamma)*(1-a),1/gamma)-destination)/(source-destination);
            values[i]=std::clamp(static_cast<int>(std::round(mapped*255)),0,255);
        }return values;
    }
};
NativeGlyphRasterizer::NativeGlyphRasterizer(QString path){owner();impl_=std::make_unique<Impl>(std::move(path));}
NativeGlyphRasterizer::~NativeGlyphRasterizer(){Q_ASSERT(QThread::currentThread()==impl_->thread);}
unsigned NativeGlyphRasterizer::colorLibraryLoads()const{owner();return impl_->colors?1u:0u;}
QImage NativeGlyphRasterizer::raster(const QList<QGlyphRun> &runs,QSize size,QColor color,QPointF origin,bool applyPreblend){
    owner();if(size.width()<1||size.width()>2048||size.height()<1||size.height()>256||!color.isValid()||
        !std::isfinite(origin.x())||!std::isfinite(origin.y())||std::abs(origin.x())>32768||std::abs(origin.y())>32768||runs.size()>4096)
        throw std::invalid_argument("Native glyph raster bounds");
    QImage result(size,QImage::Format_RGBA8888_Premultiplied);result.fill(Qt::transparent);QPainter painter(&result);
    const auto table=impl_->coverage(color);const QRect clip(QPoint{},size);
    qsizetype count=0;
    for(const auto &run:runs){count+=run.glyphIndexes().size();if(count>4096)throw std::invalid_argument("Native glyph count bounds");}
    for(const auto &qtRun:runs){const auto raw=qtRun.rawFont();const auto indexes=qtRun.glyphIndexes();const auto positions=qtRun.positions();
        if(!raw.isValid()||!std::isfinite(raw.pixelSize())||raw.pixelSize()<8||raw.pixelSize()>128||
            raw.familyName().isEmpty()||raw.familyName().size()>128||indexes.size()!=positions.size())throw std::invalid_argument("Native glyph run bounds");
        auto *face=impl_->faceFor(raw);
        for(qsizetype i=0;i<indexes.size();++i){
            if(indexes[i]>65535)throw std::invalid_argument("Native glyph index bounds");
            const auto position=positions[i]+origin;
            if(!std::isfinite(position.x())||!std::isfinite(position.y())||std::abs(position.x())>32768||std::abs(position.y())>32768)
                throw std::invalid_argument("Native glyph position bounds");
            const UINT16 glyph=static_cast<UINT16>(indexes[i]);DWRITE_GLYPH_RUN run{};run.fontFace=face;
            run.fontEmSize=static_cast<float>(raw.pixelSize());run.glyphCount=1;run.glyphIndices=&glyph;
            const float x=static_cast<float>(position.x()),y=static_cast<float>(position.y());
            ComPtr<IDWriteColorGlyphRunEnumerator> layers;
            const HRESULT colored=impl_->factory->TranslateColorGlyphRun(x,y,&run,nullptr,DWRITE_MEASURING_MODE_NATURAL,nullptr,0,&layers);
            if(colored!=DWRITE_E_NOCOLOR){require(colored);
                if(!impl_->colors)impl_->colors=std::make_unique<ColorRaster>(impl_->binary);
                painter.drawImage(0,0,impl_->colors->raster(raw,glyph,size,color,position));continue;
            }
            const float advance=0;const DWRITE_GLYPH_OFFSET offset{};run.glyphAdvances=&advance;run.glyphOffsets=&offset;
            const DWRITE_MATRIX transform{1,0,0,1,std::floor(x*4+0.5f)/4,std::round(y)};
            ComPtr<IDWriteGlyphRunAnalysis> analysis;
            require(static_cast<IDWriteFactory*>(impl_->factory.Get())->CreateGlyphRunAnalysis(&run,1,&transform,modeFor(run),DWRITE_MEASURING_MODE_NATURAL,0,0,&analysis));
            RECT bounds{};require(analysis->GetAlphaTextureBounds(DWRITE_TEXTURE_CLEARTYPE_3x1,&bounds));
            const auto rectangle=QRect(bounds.left,bounds.top,bounds.right-bounds.left,bounds.bottom-bounds.top).intersected(clip);
            if(rectangle.isEmpty())continue;bounds={rectangle.left(),rectangle.top(),rectangle.right()+1,rectangle.bottom()+1};
            std::vector<BYTE> mask(static_cast<std::size_t>(rectangle.width())*rectangle.height()*3);
            require(analysis->CreateAlphaTexture(DWRITE_TEXTURE_CLEARTYPE_3x1,&bounds,mask.data(),static_cast<UINT32>(mask.size())));
            QImage pixels(rectangle.size(),QImage::Format_RGBA8888_Premultiplied);
            for(int row=0;row<rectangle.height();++row){auto *target=pixels.scanLine(row);
                for(int column=0;column<rectangle.width();++column){const auto at=(static_cast<std::size_t>(row)*rectangle.width()+column)*3;
                    const int coverage=(mask[at]+mask[at+1]+mask[at+2])/3;
                    const int alpha=((applyPreblend?table[coverage]:coverage)*color.alpha()+127)/255;
                    target[column*4]=static_cast<uchar>((color.red()*alpha+127)/255);target[column*4+1]=static_cast<uchar>((color.green()*alpha+127)/255);
                    target[column*4+2]=static_cast<uchar>((color.blue()*alpha+127)/255);target[column*4+3]=static_cast<uchar>(alpha);
                }
            }painter.drawImage(rectangle.topLeft(),pixels);
        }
    }return result;
}
QImage NativeGlyphRasterizer::rasterShadow(const QImage &coverage,QColor color,double radius){
    owner();if(QThread::currentThread()!=impl_->thread)throw std::logic_error("Native shadow owner mismatch");
    if(coverage.isNull()||coverage.width()>2048||coverage.height()>256||coverage.format()!=QImage::Format_RGBA8888_Premultiplied||
       !color.isValid()||!std::isfinite(radius)||radius<0||radius>64)throw std::invalid_argument("Native shadow bounds");
    QImage mask(coverage.size(),QImage::Format_RGBA8888_Premultiplied);
    bool visible=false;
    for(int y=0;y<coverage.height();++y){const auto *source=coverage.constScanLine(y);auto *target=mask.scanLine(y);
        for(int x=0;x<coverage.width();++x){const auto alpha=source[x*4+3];visible|=alpha!=0&&color.alpha()!=0;
            target[x*4]=target[x*4+1]=target[x*4+2]=target[x*4+3]=alpha;
        }
    }
    if(visible&&radius>0){if(!impl_->colors)impl_->colors=std::make_unique<ColorRaster>(impl_->binary);mask=impl_->colors->blur(mask,radius);}
    QImage tinted(mask.size(),QImage::Format_RGBA8888_Premultiplied);
    for(int y=0;y<mask.height();++y){const auto *source=mask.constScanLine(y);auto *target=tinted.scanLine(y);
        for(int x=0;x<mask.width();++x){const int alpha=(source[x*4+3]*color.alpha()+127)/255;
            target[x*4]=static_cast<uchar>((color.red()*alpha+127)/255);target[x*4+1]=static_cast<uchar>((color.green()*alpha+127)/255);
            target[x*4+2]=static_cast<uchar>((color.blue()*alpha+127)/255);target[x*4+3]=static_cast<uchar>(alpha);
        }
    }return tinted;
}
}
