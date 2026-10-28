// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "playing/visual/normal-prepared.hpp"
#include <QFont>
namespace playing::visual::normal {
struct AuthoredTextStyle {
    QString family=QStringLiteral("Consolas");
    QStringList fallbackFamilies;
    bool windowsRaster=false; // Candidate comparison; not a product setting.
    bool windowsDownsample=false;
    bool windowsNatural=false;
    bool windowsGray=false;
    bool windowsSystemPreblend=false;
    QString colorPathLibrary;
    bool windowsColorGlyph=false;
    int pixelSize=29;
    QFont::Weight weight=QFont::Medium;
    double letterSpacing=0;
    QColor color=QColor(163,250,196);
    // Optional authored line box. Both zero selects backend metrics.
    double lineHeight=0,baseline=0;
};
struct PreparedAuthoredText {
    double advance=0,baseline=0;
    int height=0;
    QStringList resolvedFamilies;
    std::vector<TextTile> tiles;
};
// Native DOM font candidate. Preparation only, GUI owner; immutable bounded
// premultiplied tiles use the host's shared engine and GPU group contract.
// Does not implement CSS text shadows or certify Chromium glyph parity.
PreparedAuthoredText prepareAuthoredText(oto::visual::VisualEngine &engine,
    const QString &text,const AuthoredTextStyle &style);
} // namespace playing::visual::normal
