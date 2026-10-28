// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QGlyphRun>
#include <QImage>
#include <QColor>
namespace playing::visual::normal {
// Test-only GUI preparation. Averages DirectWrite ClearType channels and
// reads COLRv0 layers; this does not certify Chromium or COLRv1 parity.
QImage rasterWindowsGlyphs(const QList<QGlyphRun> &runs,QSize size,QColor color,QPointF origin,bool downsample=false,bool natural=false,bool gray=false,bool systemPreblend=false,const QString &colorPathLibrary={},bool colorGlyph=false);
}
