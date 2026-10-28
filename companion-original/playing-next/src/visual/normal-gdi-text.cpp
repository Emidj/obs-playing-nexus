// SPDX-License-Identifier: GPL-2.0-or-later
// Compatibility behavior audited against OBS Studio 32.2.2 obs-text/gdiplus:
// https://github.com/obsproject/obs-studio/blob/32.2.2/plugins/obs-text/gdiplus/obs-text.cpp
// Original API semantics: positive LOGFONT cell height, typographic measurement,
// newline padding removal, even texture dimensions, antialias and round outline.
#include "playing/visual/normal-gdi-text.hpp"
#define NOMINMAX
#include <windows.h>
#include <gdiplus.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
namespace playing::visual::normal {
namespace {
void status(Gdiplus::Status value) {if(value!=Gdiplus::Ok)throw std::runtime_error("GDI NORMAL rasterization failed");}
struct Runtime {
    ULONG_PTR token=0;
    Runtime(){Gdiplus::GdiplusStartupInput input;status(Gdiplus::GdiplusStartup(&token,&input,nullptr));}
    ~Runtime(){Gdiplus::GdiplusShutdown(token);}
};
struct FontContext {
    HDC dc=CreateCompatibleDC(nullptr);
    HFONT handle=nullptr;
    std::unique_ptr<Gdiplus::Font> font;
    ~FontContext(){font.reset();if(handle)DeleteObject(handle);if(dc)DeleteDC(dc);}
};
int dimension(float value,int minimum) {
    int result=std::clamp(static_cast<int>(value+.0001f),minimum,16384);
    return result+result%2;
}
}
QImage rasterGdiText(const QString &input,const GdiTextStyle &style) {
    if(style.cellHeight<8||style.cellHeight>256||style.family.isEmpty()||style.family.size()>=LF_FACESIZE
        ||!style.color.isValid()||input.contains(QChar::Null)||input.toUtf8().size()>4096)
        throw std::invalid_argument("Invalid NORMAL GDI text/style");
    // Keep shutdown outside DLL detach/loader lock and release GDI ownership
    // after each preparation. No font/DC/GDI+ token remains in a render frame.
    Runtime runtime;
    (void)runtime;
    FontContext context;if(!context.dc)throw std::runtime_error("NORMAL font DC failed");
    LOGFONTW request{};request.lfHeight=style.cellHeight;request.lfWeight=style.bold?FW_BOLD:FW_DONTCARE;
    request.lfItalic=style.italic?TRUE:FALSE;request.lfUnderline=style.underline?TRUE:FALSE;request.lfStrikeOut=style.strikeout?TRUE:FALSE;request.lfQuality=ANTIALIASED_QUALITY;request.lfCharSet=DEFAULT_CHARSET;
    const auto family=style.family.toStdWString();std::copy(family.begin(),family.end(),request.lfFaceName);
    context.handle=CreateFontIndirectW(&request);if(!context.handle)throw std::runtime_error("NORMAL font creation failed");
    context.font=std::make_unique<Gdiplus::Font>(context.dc,context.handle);status(context.font->GetLastStatus());
    Gdiplus::Graphics measure(context.dc);
    Gdiplus::StringFormat format(Gdiplus::StringFormat::GenericTypographic());
    status(format.SetFormatFlags(Gdiplus::StringFormatFlagsNoFitBlackBox|Gdiplus::StringFormatFlagsMeasureTrailingSpaces));
    status(format.SetTrimming(Gdiplus::StringTrimmingWord));
    Gdiplus::RectF box;float measuredWidth=0,measuredHeight=0;
    auto text=input.toStdWString();if(!text.empty()&&text.back()!=L'\n')text.push_back(L'\n');
    if(!text.empty()) {
        status(measure.MeasureString(text.c_str(),static_cast<INT>(text.size()+1),context.font.get(),Gdiplus::PointF{},&format,&box));
        Gdiplus::RectF before,after;
        status(measure.MeasureString(L"W",2,context.font.get(),Gdiplus::PointF{},&format,&before));
        status(measure.MeasureString(L"W\n",3,context.font.get(),Gdiplus::PointF{},&format,&after));
        const float padding=after.Width-before.Width;
        measuredWidth=box.Width-(padding>=1?padding-1:padding)+(style.outline?2.0f:0.0f);
        measuredHeight=box.Height-(after.Height-before.Height)+(style.outline?2.0f:0.0f);
    }
    int width=dimension(measuredWidth,2),height=dimension(std::max(measuredHeight,static_cast<float>(style.cellHeight)),2);
    if(static_cast<std::int64_t>(width)*height>4096LL*4096LL) {
        if(width>height)width=static_cast<int>(4096LL*4096LL/height);
        else height=static_cast<int>(4096LL*4096LL/width);
    }
    box.X=0;box.Y=0;
    QImage image(width,height,QImage::Format_ARGB32);if(image.isNull())throw std::runtime_error("NORMAL text allocation failed");image.fill(Qt::transparent);
    Gdiplus::Bitmap bitmap(width,height,image.bytesPerLine(),PixelFormat32bppARGB,image.bits());status(bitmap.GetLastStatus());
    Gdiplus::Graphics painter(&bitmap);status(painter.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias));status(painter.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias));
    // OBS uses this brush even for a uniform color. GDI coverage differs from
    // SolidBrush at some glyph edges, so retain its original raster semantics.
    Gdiplus::LinearGradientBrush fill(Gdiplus::RectF(0,0,static_cast<float>(width),static_cast<float>(height)),
        Gdiplus::Color(style.color.rgba()),Gdiplus::Color(style.color.rgba()),0,TRUE);
    status(fill.GetLastStatus());
    if(!text.empty()) {
        if(style.outline) {
            box.X+=1;box.Y+=1;Gdiplus::FontFamily nativeFamily;status(context.font->GetFamily(&nativeFamily));
            Gdiplus::GraphicsPath path;status(path.AddString(text.c_str(),static_cast<INT>(text.size()),&nativeFamily,context.font->GetStyle(),context.font->GetSize(),box,&format));
            Gdiplus::Pen edge(Gdiplus::Color(191,0,0,0),2);status(edge.SetLineJoin(Gdiplus::LineJoinRound));status(painter.DrawPath(&edge,&path));status(painter.FillPath(&fill,&path));
        } else status(painter.DrawString(text.c_str(),static_cast<INT>(text.size()),context.font.get(),box,&format,&fill));
    }
    painter.Flush(Gdiplus::FlushIntentionSync);status(painter.GetLastStatus());return image;
}
} // namespace playing::visual::normal
