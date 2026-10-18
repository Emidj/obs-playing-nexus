// SPDX-License-Identifier: GPL-2.0-or-later
// Test-only C boundary over the public API of pinned upstream Skia 127.
#include "include/core/SkCanvas.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkSurface.h"
#include <cstdint>
#include <cstddef>
#include <cmath>
struct PathCommand {std::uint32_t verb;float points[6];};
extern "C" __declspec(dllexport) int pnx_skia127_path(
    const PathCommand *commands,std::size_t count,void *pixels,std::size_t byteCount,
    int width,int height,std::size_t rowBytes,std::uint32_t color,int evenOdd,float dx,float dy) noexcept {
    try{
        if(!pixels||width<1||width>2048||height<1||height>256||rowBytes<static_cast<std::size_t>(width)*4||rowBytes>8192||
           byteCount<rowBytes*static_cast<std::size_t>(height)||count>4096||(count&&!commands)||!std::isfinite(dx)||!std::isfinite(dy))return 1;
        SkPath path;path.setFillType(evenOdd?SkPathFillType::kEvenOdd:SkPathFillType::kWinding);
        for(std::size_t i=0;i<count;++i){const auto &c=commands[i];for(float p:c.points)if(!std::isfinite(p)||std::abs(p)>32768)return 2;
            switch(c.verb){case 0:path.moveTo(c.points[0],c.points[1]);break;case 1:path.lineTo(c.points[0],c.points[1]);break;
                case 2:path.cubicTo(c.points[0],c.points[1],c.points[2],c.points[3],c.points[4],c.points[5]);break;
                case 3:path.close();break;default:return 3;}}
        auto surface=SkSurfaces::WrapPixels(SkImageInfo::Make(width,height,kRGBA_8888_SkColorType,kPremul_SkAlphaType),pixels,rowBytes);
        if(!surface)return 4;auto canvas=surface->getCanvas();canvas->clear(SK_ColorTRANSPARENT);canvas->translate(dx,dy);
        SkPaint paint;paint.setAntiAlias(true);paint.setColor(color);canvas->drawPath(path,paint);return 0;
    }catch(...){return 5;}
}
