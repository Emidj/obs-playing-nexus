// SPDX-License-Identifier: GPL-2.0-or-later
// Test-only native color glyph diagnostic; public audited SkiaSharp C API.
#include <QImage>
#include <QColor>
#include <QPointF>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QLibrary>
#include <QScopeGuard>
#include <stdexcept>
#include "include/c/sk_font.h"
#include "include/c/sk_typeface.h"
#include "include/c/sk_surface.h"
#include "include/c/sk_canvas.h"
#include "include/c/sk_paint.h"
#include "include/c/sk_textblob.h"
namespace playing::visual::normal {
QImage rasterSkiaColorGlyph(const QString &family,int weight,float pixelSize,std::uint16_t glyph,QSize size,QColor color,QPointF origin,const QString &binaryPath){
    if(!QFileInfo(binaryPath).isAbsolute()||size.width()<1||size.width()>2048||size.height()<1||size.height()>256||pixelSize<8||pixelSize>128)
        throw std::invalid_argument("Color glyph diagnostic bounds");
    QFile file(binaryPath);if(!file.open(QIODevice::ReadOnly))throw std::runtime_error("Color glyph library unavailable");
    QCryptographicHash hash(QCryptographicHash::Sha256);if(!hash.addData(&file)||hash.result().toHex()!=QByteArrayLiteral("5fffeed011731a3255022f003045995f5e96810ca5538c5fe4a91b048b32560e"))
        throw std::runtime_error("Color glyph library hash mismatch");
    file.close();QLibrary library(binaryPath);if(!library.load())throw std::runtime_error("Color glyph library load failed");
#define LOAD(name) const auto name=reinterpret_cast<decltype(&::name)>(library.resolve(#name));if(!name)throw std::runtime_error(#name)
    LOAD(sk_fontmgr_create_default);LOAD(sk_fontmgr_unref);LOAD(sk_fontmgr_match_family_style);LOAD(sk_fontstyle_new);LOAD(sk_fontstyle_delete);LOAD(sk_typeface_unref);
    LOAD(sk_font_new_with_values);LOAD(sk_font_delete);LOAD(sk_font_set_hinting);LOAD(sk_font_set_edging);LOAD(sk_font_set_subpixel);LOAD(sk_font_set_linear_metrics);LOAD(sk_font_set_baseline_snap);
    LOAD(sk_surface_new_raster_direct);LOAD(sk_surface_get_canvas);LOAD(sk_surface_unref);
    LOAD(sk_textblob_builder_new);LOAD(sk_textblob_builder_delete);LOAD(sk_textblob_builder_alloc_run_pos);LOAD(sk_textblob_builder_make);LOAD(sk_textblob_unref);
    LOAD(sk_paint_new);LOAD(sk_paint_delete);LOAD(sk_paint_set_color);LOAD(sk_paint_set_antialias);LOAD(sk_canvas_draw_text_blob);
#undef LOAD
    auto manager=sk_fontmgr_create_default();if(!manager)throw std::runtime_error("Color font manager unavailable");const auto managerGuard=qScopeGuard([&]{sk_fontmgr_unref(manager);});
    auto style=sk_fontstyle_new(weight,5,UPRIGHT_SK_FONT_STYLE_SLANT);if(!style)throw std::runtime_error("Color font style unavailable");const auto styleGuard=qScopeGuard([&]{sk_fontstyle_delete(style);});
    auto face=sk_fontmgr_match_family_style(manager,family.toUtf8().constData(),style);if(!face)throw std::runtime_error("Color typeface unavailable");const auto faceGuard=qScopeGuard([&]{sk_typeface_unref(face);});
    auto font=sk_font_new_with_values(face,pixelSize,1,0);if(!font)throw std::runtime_error("Color font unavailable");const auto fontGuard=qScopeGuard([&]{sk_font_delete(font);});
    sk_font_set_hinting(font,NORMAL_SK_FONT_HINTING);sk_font_set_edging(font,ANTIALIAS_SK_FONT_EDGING);sk_font_set_subpixel(font,true);sk_font_set_linear_metrics(font,true);sk_font_set_baseline_snap(font,false);
    QImage pixels(size,QImage::Format_RGBA8888_Premultiplied);pixels.fill(Qt::transparent);
    sk_imageinfo_t info{nullptr,size.width(),size.height(),RGBA_8888_SK_COLORTYPE,PREMUL_SK_ALPHATYPE};
    auto surface=sk_surface_new_raster_direct(&info,pixels.bits(),pixels.bytesPerLine(),nullptr,nullptr,nullptr);if(!surface)throw std::runtime_error("Color surface unavailable");const auto surfaceGuard=qScopeGuard([&]{sk_surface_unref(surface);});
    auto paint=sk_paint_new();if(!paint)throw std::runtime_error("Color paint unavailable");const auto paintGuard=qScopeGuard([&]{sk_paint_delete(paint);});
    sk_paint_set_color(paint,color.rgba());sk_paint_set_antialias(paint,true);
    auto builder=sk_textblob_builder_new();if(!builder)throw std::runtime_error("Color text builder unavailable");const auto builderGuard=qScopeGuard([&]{sk_textblob_builder_delete(builder);});
    sk_textblob_builder_runbuffer_t buffer{};sk_textblob_builder_alloc_run_pos(builder,font,1,nullptr,&buffer);
    if(!buffer.glyphs||!buffer.pos)throw std::runtime_error("Color text storage unavailable");
    *static_cast<std::uint16_t*>(buffer.glyphs)=glyph;auto points=static_cast<float*>(buffer.pos);points[0]=static_cast<float>(origin.x());points[1]=static_cast<float>(origin.y());
    auto blob=sk_textblob_builder_make(builder);if(!blob)throw std::runtime_error("Color glyph blob unavailable");const auto blobGuard=qScopeGuard([&]{sk_textblob_unref(blob);});
    sk_canvas_draw_text_blob(sk_surface_get_canvas(surface),blob,0,0,paint);return pixels;
}
}
