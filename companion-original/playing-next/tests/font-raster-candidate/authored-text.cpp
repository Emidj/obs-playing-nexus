// SPDX-License-Identifier: GPL-2.0-or-later
#include "playing/visual/authored-text.hpp"
#include <QGuiApplication>
#include <QTextLayout>
#include <QPainter>
#include <QThread>
#include <QGlyphRun>
#include "playing/visual/windows-glyph-raster.hpp"
namespace playing::visual::normal {
PreparedAuthoredText prepareAuthoredText(oto::visual::VisualEngine &engine,
    const QString &text,const AuthoredTextStyle &style) {
    if(!qobject_cast<QGuiApplication*>(QCoreApplication::instance())||QThread::currentThread()!=qApp->thread())
        throw std::logic_error("Authored text preparation requires GUI owner");
    if(text.size()>4096||text.toUtf8().size()>4096||text.contains(QChar('\n'))||text.contains(QChar('\r'))||
       style.family.isEmpty()||style.family.size()>128||
       style.pixelSize<8||style.pixelSize>128||!std::isfinite(style.letterSpacing)||
       std::abs(style.letterSpacing)>8||!style.color.isValid()||
       style.weight<QFont::Thin||style.weight>QFont::Black)
        throw std::invalid_argument("Authored text preparation bounds");
    if(!std::isfinite(style.lineHeight)||!std::isfinite(style.baseline)||style.lineHeight<0||style.lineHeight>256||
       style.baseline<0||style.baseline>style.lineHeight||((style.lineHeight==0)!=(style.baseline==0)))
        throw std::invalid_argument("Authored text line box bounds");
    if(style.fallbackFamilies.size()>8)throw std::invalid_argument("Authored font fallback bounds");
    for(const auto &family:style.fallbackFamilies)if(family.isEmpty()||family.size()>128)
        throw std::invalid_argument("Authored font fallback bounds");
    // QTextLayout stores advances in 1/64 units. Accumulate at higher precision
    // and normalize shaped positions before painting with a pixel-size face.
    // This avoids a per-glyph truncation drift on long authored DOM text.
    // Conservatively cap the internal font too: an uncapped 128px fixture
    // produced no ink. Output size remains the caller's original 8–128 pixels.
    const double precision=std::min(1024,32768/style.pixelSize);
    QFont font(style.family);font.setPixelSize(static_cast<int>(style.pixelSize*precision));font.setWeight(style.weight);
    if(!style.fallbackFamilies.isEmpty()){auto families=style.fallbackFamilies;families.prepend(style.family);font.setFamilies(families);}
    font.setHintingPreference(QFont::PreferNoHinting);
    font.setLetterSpacing(QFont::AbsoluteSpacing,style.letterSpacing*precision);
    QTextLayout layout(text.isEmpty()?QString(QChar(0xa0)):text,font);
    QTextOption option;option.setWrapMode(QTextOption::NoWrap);layout.setTextOption(option);
    layout.beginLayout();auto line=layout.createLine();
    if(!line.isValid()){layout.endLayout();throw std::invalid_argument("Authored text line unavailable");}
    line.setLineWidth(16384*precision);layout.endLayout();
    PreparedAuthoredText result;result.advance=line.naturalTextWidth()/precision;result.baseline=line.ascent()/precision;
    result.height=static_cast<int>(std::ceil(line.height()/precision));
    const double backendBaseline=result.baseline;
    if(style.lineHeight){result.height=static_cast<int>(std::ceil(style.lineHeight));result.baseline=style.baseline;}
    if(!std::isfinite(result.advance)||result.advance<0||result.advance>16382||result.height<1||result.height>256)
        throw std::invalid_argument("Authored text raster bounds");
    auto glyphs=layout.glyphRuns();
    for(auto &run:glyphs){
        const auto original=run.rawFont();QFont rasterFont(original.familyName());rasterFont.setStyleName(original.styleName());
        if(!result.resolvedFamilies.contains(original.familyName()))result.resolvedFamilies.push_back(original.familyName());
        rasterFont.setPixelSize(style.pixelSize);rasterFont.setHintingPreference(QFont::PreferVerticalHinting);
        auto face=QRawFont::fromFont(rasterFont);if(!face.isValid())throw std::runtime_error("Authored glyph face unavailable");run.setRawFont(face);
        auto positions=run.positions();for(auto &position:positions)position/=precision;run.setPositions(positions);
    }
    // Raster only each bounded tile; neighboring gutters preserve filtering.
    const int width=std::max(1,static_cast<int>(std::ceil(result.advance))+2);
    for(int x=0;x<width;x+=2046){
        const int content=std::min(2046,width-x),left=std::max(0,x-1),right=std::min(width,x+content+1);
        QImage pixels(right-left,result.height,QImage::Format_RGBA8888_Premultiplied);pixels.fill(Qt::transparent);
        if(!text.isEmpty()&&style.windowsRaster){
            pixels=rasterWindowsGlyphs(glyphs,pixels.size(),style.color,QPointF(-left,result.baseline-backendBaseline),style.windowsDownsample,style.windowsNatural,style.windowsGray,style.windowsSystemPreblend,style.colorPathLibrary,style.windowsColorGlyph);
        }else if(!text.isEmpty()){
            QPainter painter(&pixels);painter.setRenderHint(QPainter::TextAntialiasing);painter.setPen(style.color);
            for(const auto &run:glyphs)painter.drawGlyphRun(QPointF(-left,result.baseline-backendBaseline),run);
        }
        result.tiles.push_back({left,x,content,engine.prepareRaster(std::move(pixels))});
    }
    return result;
}
} // namespace playing::visual::normal
