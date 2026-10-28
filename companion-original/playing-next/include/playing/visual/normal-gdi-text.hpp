// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QImage>
#include <QColor>
#include <QString>
namespace playing::visual::normal {
struct GdiTextStyle {
    QString family=QStringLiteral("Segoe UI");
    int cellHeight=36;
    bool bold=true,italic=false,outline=false;
    bool underline=false,strikeout=false;
    QColor color=Qt::white;
};
// Windows rasterization for original GDI designs and bounded native redesigns.
// No OBS child sources or graphics operations. Returned pixels are straight
// ARGB, to be prepared by the common VisualEngine outside video_render.
QImage rasterGdiText(const QString &text,const GdiTextStyle &style);
} // namespace playing::visual::normal
