// SPDX-License-Identifier: GPL-2.0-or-later
// Isolated diagnostic using the audited public C API; never linked to product.
#include <QPainterPath>
#include <QPainter>
#include <QImage>
#include <QColor>
#include <QLibrary>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QScopeGuard>
#include <stdexcept>
#include <vector>
#include <cstdint>
#include "include/c/sk_path.h"
#include "include/c/sk_pathbuilder.h"
#include "include/c/sk_surface.h"
#include "include/c/sk_canvas.h"
#include "include/c/sk_paint.h"
namespace playing::visual::normal {
template<class Handle,class Move,class Line,class Curve,class Close>
void appendOutline(const QPainterPath &outline,QPointF origin,Handle handle,Move move,Line line,Curve curve,Close close){
    QPointF start;
    for(int i=0;i<outline.elementCount();++i){const auto e=outline.elementAt(i);
        const auto x=static_cast<float>(e.x+origin.x()),y=static_cast<float>(e.y+origin.y());
        if(e.isMoveTo()){if(i)close(handle);start={e.x,e.y};move(handle,x,y);}
        else if(e.isLineTo()){const bool closing=QPointF(e.x,e.y)==start&&(i+1==outline.elementCount()||outline.elementAt(i+1).isMoveTo());if(!closing)line(handle,x,y);}
        else if(e.type==QPainterPath::CurveToElement){if(i+2>=outline.elementCount())throw std::invalid_argument("Incomplete path curve");
            const auto b=outline.elementAt(++i),c=outline.elementAt(++i);
            curve(handle,x,y,static_cast<float>(b.x+origin.x()),static_cast<float>(b.y+origin.y()),static_cast<float>(c.x+origin.x()),static_cast<float>(c.y+origin.y()));}
        else throw std::invalid_argument("Unexpected path element");}
    close(handle);
}
QImage rasterSkiaColorPath(const QPainterPath &outline,QSize size,QColor color,QPointF origin,const QString &binaryPath){
    if(!QFileInfo(binaryPath).isAbsolute())throw std::invalid_argument("Absolute audited path raster library required");
    QFile file(binaryPath);if(!file.open(QIODevice::ReadOnly))throw std::runtime_error("Path raster library unavailable");
    QCryptographicHash hash(QCryptographicHash::Sha256);if(!hash.addData(&file))throw std::runtime_error("Path library hash unavailable");
    const auto digest=hash.result().toHex();const bool legacy=digest==QByteArrayLiteral("de22ab3e7b83644614efb55da18ef0965c3976ba4df99017f1025fbdb1893ff1");
    const bool exact127=digest==QByteArrayLiteral("cfc6b4b0858f38ae4dff021afd21894e678022f59e95528b7e93773bc4f5dab2")||digest==QByteArrayLiteral("7cefb3a43b33b148bfe8e3593ad3c0e86e4590cdd5d6ee01506686a5d054ba48");
    if(!legacy&&!exact127&&digest!=QByteArrayLiteral("5fffeed011731a3255022f003045995f5e96810ca5538c5fe4a91b048b32560e"))
        throw std::runtime_error("Path raster library hash mismatch");
    file.close();QLibrary library(binaryPath);if(!library.load())throw std::runtime_error("Path raster library load failed");
    if(exact127){
        struct Command {std::uint32_t verb;float points[6];};static_assert(sizeof(Command)==28);
        using Raster=int(*)(const Command*,std::size_t,void*,std::size_t,int,int,std::size_t,std::uint32_t,int,float,float);
        const auto raster=reinterpret_cast<Raster>(library.resolve("pnx_skia127_path"));if(!raster)throw std::runtime_error("Skia127 path boundary unavailable");
        const auto bounds=outline.controlPointRect().translated(origin).toAlignedRect().intersected(QRect(QPoint{},size));
        QImage output(size,QImage::Format_RGBA8888_Premultiplied);output.fill(Qt::transparent);if(bounds.isEmpty())return output;
        QImage pixels(bounds.size(),QImage::Format_RGBA8888_Premultiplied);pixels.fill(Qt::transparent);std::vector<Command> commands;QPointF start;
        for(int i=0;i<outline.elementCount();++i){const auto e=outline.elementAt(i);Command command{};
            command.points[0]=static_cast<float>(e.x);command.points[1]=static_cast<float>(e.y);
            if(e.isMoveTo()){if(i)commands.push_back({3,{}});start={e.x,e.y};command.verb=0;}
            else if(e.isLineTo()){const bool closing=QPointF(e.x,e.y)==start&&(i+1==outline.elementCount()||outline.elementAt(i+1).isMoveTo());if(closing)continue;command.verb=1;}
            else if(e.type==QPainterPath::CurveToElement){if(i+2>=outline.elementCount())throw std::invalid_argument("Incomplete native curve");
                command.verb=2;const auto b=outline.elementAt(++i),c=outline.elementAt(++i);
                command.points[2]=static_cast<float>(b.x);command.points[3]=static_cast<float>(b.y);command.points[4]=static_cast<float>(c.x);command.points[5]=static_cast<float>(c.y);}
            else throw std::invalid_argument("Unexpected native path element");commands.push_back(command);}
        commands.push_back({3,{}});
        if(raster(commands.data(),commands.size(),pixels.bits(),static_cast<std::size_t>(pixels.sizeInBytes()),pixels.width(),pixels.height(),static_cast<std::size_t>(pixels.bytesPerLine()),color.rgba(),outline.fillRule()==Qt::OddEvenFill,static_cast<float>(origin.x()-bounds.x()),static_cast<float>(origin.y()-bounds.y())))
            throw std::runtime_error("Skia127 path raster rejected");
        QPainter painter(&output);painter.drawImage(bounds.topLeft(),pixels);painter.end();return output;
    }
#define RESOLVE(name) const auto name=reinterpret_cast<decltype(&::name)>(library.resolve(#name));if(!name)throw std::runtime_error(#name)
    RESOLVE(sk_path_delete);RESOLVE(sk_path_set_filltype);
    RESOLVE(sk_surface_new_raster_direct);RESOLVE(sk_surface_get_canvas);RESOLVE(sk_surface_unref);
    RESOLVE(sk_paint_new);RESOLVE(sk_paint_delete);RESOLVE(sk_paint_set_color);RESOLVE(sk_paint_set_antialias);RESOLVE(sk_canvas_draw_path);RESOLVE(sk_canvas_translate);
    const auto bounds=outline.controlPointRect().translated(origin).toAlignedRect().intersected(QRect(QPoint{},size));
    QImage output(size,QImage::Format_RGBA8888_Premultiplied);output.fill(Qt::transparent);if(bounds.isEmpty())return output;
    QImage pixels(bounds.size(),QImage::Format_RGBA8888_Premultiplied);pixels.fill(Qt::transparent);
    sk_imageinfo_t info{nullptr,pixels.width(),pixels.height(),RGBA_8888_SK_COLORTYPE,PREMUL_SK_ALPHATYPE};
    auto surface=sk_surface_new_raster_direct(&info,pixels.bits(),pixels.bytesPerLine(),nullptr,nullptr,nullptr);
    if(!surface)throw std::runtime_error("Path surface unavailable");const auto surfaceGuard=qScopeGuard([&]{sk_surface_unref(surface);});
    sk_canvas_translate(sk_surface_get_canvas(surface),static_cast<float>(origin.x()-bounds.x()),static_cast<float>(origin.y()-bounds.y()));
    const auto finish=[&]{QPainter painter(&output);painter.drawImage(bounds.topLeft(),pixels);painter.end();return output;};
    auto paint=sk_paint_new();if(!paint)throw std::runtime_error("Native paint unavailable");const auto paintGuard=qScopeGuard([&]{sk_paint_delete(paint);});
    sk_paint_set_color(paint,color.rgba());sk_paint_set_antialias(paint,true);
    if(legacy){
        // Public 3.119.1 C API signatures checked against mono/skia commit
        // 40f75dc0 include/c/sk_path.h; only opaque public handles are used.
        const auto oldNew=reinterpret_cast<sk_path_t*(*)()>(library.resolve("sk_path_new"));
        const auto oldMove=reinterpret_cast<void(*)(sk_path_t*,float,float)>(library.resolve("sk_path_move_to"));
        const auto oldLine=reinterpret_cast<void(*)(sk_path_t*,float,float)>(library.resolve("sk_path_line_to"));
        const auto oldCurve=reinterpret_cast<void(*)(sk_path_t*,float,float,float,float,float,float)>(library.resolve("sk_path_cubic_to"));
        const auto oldClose=reinterpret_cast<void(*)(sk_path_t*)>(library.resolve("sk_path_close"));
        if(!oldNew||!oldMove||!oldLine||!oldCurve||!oldClose)throw std::runtime_error("Legacy path C API unavailable");
        auto path=oldNew();if(!path)throw std::runtime_error("Legacy path unavailable");const auto pathGuard=qScopeGuard([&]{sk_path_delete(path);});
        sk_path_set_filltype(path,outline.fillRule()==Qt::WindingFill?WINDING_SK_PATH_FILLTYPE:EVENODD_SK_PATH_FILLTYPE);
        appendOutline(outline,QPointF{},path,oldMove,oldLine,oldCurve,oldClose);
        sk_canvas_draw_path(sk_surface_get_canvas(surface),path,paint);return finish();
    }
    RESOLVE(sk_pathbuilder_new);RESOLVE(sk_pathbuilder_delete);RESOLVE(sk_pathbuilder_move_to);RESOLVE(sk_pathbuilder_line_to);RESOLVE(sk_pathbuilder_cubic_to);RESOLVE(sk_pathbuilder_close);RESOLVE(sk_pathbuilder_set_filltype);RESOLVE(sk_pathbuilder_detach_path);
#undef RESOLVE
    auto builder=sk_pathbuilder_new();if(!builder)throw std::runtime_error("Native path unavailable");const auto builderGuard=qScopeGuard([&]{sk_pathbuilder_delete(builder);});
    sk_pathbuilder_set_filltype(builder,outline.fillRule()==Qt::WindingFill?WINDING_SK_PATH_FILLTYPE:EVENODD_SK_PATH_FILLTYPE);
    appendOutline(outline,QPointF{},builder,sk_pathbuilder_move_to,sk_pathbuilder_line_to,sk_pathbuilder_cubic_to,sk_pathbuilder_close);
    auto path=sk_pathbuilder_detach_path(builder);if(!path)throw std::runtime_error("Native path result unavailable");const auto pathGuard=qScopeGuard([&]{sk_path_delete(path);});
    sk_canvas_draw_path(sk_surface_get_canvas(surface),path,paint);return finish();
}
}
