// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "authored-text.hpp"
#include <memory>
namespace playing::visual::normal {
// One instance per GUI host. Native resources are prepared and released on
// that owner; the renderer receives only the immutable engine's raster tiles.
class NativeGlyphRasterizer final {
public:
    explicit NativeGlyphRasterizer(QString colorLibrary);
    ~NativeGlyphRasterizer();
    NativeGlyphRasterizer(const NativeGlyphRasterizer &)=delete;
    NativeGlyphRasterizer &operator=(const NativeGlyphRasterizer &)=delete;
    QImage raster(const QList<QGlyphRun> &,QSize,QColor,QPointF,bool applyPreblend=true);
    // GUI preparation only. Caller reserves transparent padding in the mask.
    // CSS blur radius maps to sigma=radius/2, as Chromium127 specifies.
    QImage rasterShadow(const QImage &coverage,QColor,double blurRadius);
    unsigned colorLibraryLoads() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
