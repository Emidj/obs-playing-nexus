// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "normal-prepared.hpp"
#include <QFont>
#include <QGlyphRun>
#include <functional>
namespace playing::visual::normal {
struct AuthoredTextStyle {
    TextEffects effects;
    QString family=QStringLiteral("Consolas");
    QStringList fallbackFamilies;
    int pixelSize=29;
    QFont::Weight weight=QFont::Medium;
    bool italic=false;
    double letterSpacing=0;
    QColor color=QColor(163,250,196);
    // Both zero selects the backend line box. Otherwise use authored metrics.
    double lineHeight=0,baseline=0;
    // Preserve glyph ink outside its line box for effects; geometry is unchanged.
    int inkPadding=0;
    // Keep pathological valid metadata bounded for native redirection. Core
    // remains intact; text beyond the 16K-pixel visual budget ends in ellipsis.
    bool boundedEllipsis=false;
};
struct PreparedAuthoredText {
    double advance=0,baseline=0;
    int height=0;
    int inkPadding=0;
    QStringList resolvedFamilies;
    std::vector<TextTile> tiles;
};
// Host-owned native backend, called only during bounded GUI preparation.
// Positions and font faces have already been normalized to output pixels.
// Returns exactly the requested size and RGBA8888_Premultiplied encoding.
using AuthoredGlyphRaster = std::function<QImage(const QList<QGlyphRun> &,
    QSize,QColor,QPointF)>;
// Immutable tiles use the host's existing shared engine. An empty backend
// keeps Qt preparation. Backend resource initialization belongs on this GUI
// path; no clock or GPU calls, and never invoke the backend while drawing.
PreparedAuthoredText prepareAuthoredText(oto::visual::VisualEngine &engine,
    const QString &text,const AuthoredTextStyle &style,
    const AuthoredGlyphRaster &raster={});
struct AuthoredTrackTextStyle {
    std::array<AuthoredTextStyle,3> rows;
    std::array<bool,3> uppercase{};
};
struct PreparedAuthoredTrackText {
    TrackPtr track;
    std::array<PreparedAuthoredText,3> rows;
};
using PreparedAuthoredTrackTextPtr=std::shared_ptr<const PreparedAuthoredTrackText>;
// Display casing belongs to the consumer; original Track State remains const.
PreparedAuthoredTrackTextPtr prepareAuthoredTrackText(oto::visual::VisualEngine &engine,
    TrackPtr track,const QString &label,const AuthoredTrackTextStyle &style,
    const AuthoredGlyphRaster &raster={});
} // namespace playing::visual::normal
