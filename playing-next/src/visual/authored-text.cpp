// SPDX-License-Identifier: GPL-2.0-or-later
#include "playing/visual/authored-text.hpp"
#include <QGuiApplication>
#include <QTextLayout>
#include <QFontMetricsF>
#include <QTextCharFormat>
#include <QPainter>
#include <QThread>
namespace playing::visual::normal {
PreparedAuthoredTrackTextPtr prepareAuthoredTrackText(oto::visual::VisualEngine &engine,
    TrackPtr track,const QString &label,const AuthoredTrackTextStyle &style,
    const AuthoredGlyphRaster &raster) {
    if(!track||!core::validMetadata(*track))throw std::invalid_argument("Invalid authored NORMAL track");
    auto result=std::make_shared<PreparedAuthoredTrackText>();result->track=std::move(track);
    std::array<QString,3> text{label,QString::fromUtf8(result->track->title.c_str()),QString::fromUtf8(result->track->artist.c_str())};
    for(std::size_t i=0;i<text.size();++i)
        result->rows[i]=prepareAuthoredText(engine,style.uppercase[i]?text[i].toUpper():text[i],style.rows[i],raster);
    return result;
}
PreparedAuthoredText prepareAuthoredText(oto::visual::VisualEngine &engine,
    const QString &text,const AuthoredTextStyle &style,const AuthoredGlyphRaster &raster) {
    if(!qobject_cast<QGuiApplication*>(QCoreApplication::instance())||QThread::currentThread()!=qApp->thread())
        throw std::logic_error("Authored text preparation requires GUI owner");
    // Uppercase presentation can expand a valid Core string by up to three times.
    const int textLimit=style.boundedEllipsis?12288:4096;
    if(text.size()>textLimit||text.toUtf8().size()>textLimit||text.contains(QChar('\n'))||text.contains(QChar('\r'))||
       style.family.isEmpty()||style.family.size()>128||
       style.pixelSize<8||style.pixelSize>128||!std::isfinite(style.letterSpacing)||
       std::abs(style.letterSpacing)>8||!style.color.isValid()||
       style.weight<QFont::Thin||style.weight>QFont::Black||style.inkPadding<0||style.inkPadding>32)
        throw std::invalid_argument("Authored text preparation bounds");
    if(!std::isfinite(style.lineHeight)||!std::isfinite(style.baseline)||style.lineHeight<0||style.lineHeight>256||
       style.baseline<0||style.baseline>style.lineHeight||((style.lineHeight==0)!=(style.baseline==0)))
        throw std::invalid_argument("Authored text line box bounds");
    if(style.fallbackFamilies.size()>8)throw std::invalid_argument("Authored font fallback bounds");
    for(const auto &family:style.fallbackFamilies)if(family.isEmpty()||family.size()>128)
        throw std::invalid_argument("Authored font fallback bounds");
    // QTextLayout stores advances in 1/64 units. Accumulate at higher precision
    // and scale the shaped run at painting, not the bitmap or final metadata.
    // This avoids a per-glyph truncation drift on long authored DOM text.
    // Conservatively cap the internal font too: an uncapped 128px fixture
    // produced no ink. Output size remains the caller's original 8–128 pixels.
    // Binary scaling keeps TrueType design advances exact through Qt's 26.6
    // positions. A nonbinary divisor can move quarter-pixel mask boundaries.
    int precision=1024;
    while(style.pixelSize*precision>32768)precision/=2;
    // Native redesigns use the actual output font size. Oversized reference
    // fonts are unnecessary here and can clip color-font bitmap strikes.
    // This also keeps Qt's finite 26.6 span safe for maximum valid metadata.
    if(style.boundedEllipsis)precision=1;
    QFont font(style.family);font.setPixelSize(static_cast<int>(style.pixelSize*precision));font.setWeight(style.weight);
    font.setItalic(style.italic);
    if(!style.fallbackFamilies.isEmpty()){auto families=style.fallbackFamilies;families.prepend(style.family);font.setFamilies(families);}
    font.setHintingPreference(QFont::PreferNoHinting);
    font.setLetterSpacing(QFont::AbsoluteSpacing,style.letterSpacing*precision);
    QTextLayout layout(text.isEmpty()?QString(QChar(0xa0)):text,font);
    const auto nativeFallback=[&]{
        if(!style.boundedEllipsis)return;
        QList<QTextLayout::FormatRange> ranges;
        QFont fallback=font;fallback.setItalic(false);fallback.setWeight(QFont::Normal);
        fallback.setFamilies({QStringLiteral("Segoe UI Emoji"),QStringLiteral("Segoe UI Symbol"),QStringLiteral("Microsoft YaHei")});
        const auto content=layout.text();
        for(qsizetype i=0;i+1<content.size();++i)if(content[i].isHighSurrogate()&&content[i+1].isLowSurrogate()){
            QTextLayout::FormatRange range;range.start=static_cast<int>(i);range.length=2;range.format.setFont(fallback);ranges.push_back(range);++i;
        }
        // Bitmap emoji faces have no italic/bold strikes. Preserve the title's
        // styling around them rather than synthesizing clipped color glyphs.
        layout.setFormats(ranges);
    };
    nativeFallback();
    QTextOption option;option.setWrapMode(QTextOption::NoWrap);layout.setTextOption(option);
    layout.beginLayout();auto line=layout.createLine();
    if(!line.isValid()){layout.endLayout();throw std::invalid_argument("Authored text line unavailable");}
    line.setLineWidth(16384*precision);layout.endLayout();
    if(style.boundedEllipsis&&line.horizontalAdvance()/precision>16380) {
        layout.setText(QFontMetricsF(font).elidedText(layout.text(),Qt::ElideRight,16000.0*precision));
        nativeFallback();
        layout.beginLayout();line=layout.createLine();
        if(!line.isValid()){layout.endLayout();throw std::invalid_argument("Bounded native text line unavailable");}
        line.setLineWidth(16384*precision);layout.endLayout();
    }
    // CSS max-content follows glyph advances, not italic ink overhang.
    style.effects.validate();const int effectPad=style.effects.padding();
    PreparedAuthoredText result;result.advance=line.horizontalAdvance()/precision;result.baseline=line.ascent()/precision;
    result.height=static_cast<int>(std::ceil(line.height()/precision));
    const double backendBaseline=result.baseline;
    if(style.lineHeight){result.height=static_cast<int>(std::ceil(style.lineHeight));result.baseline=style.baseline;}
    if(!std::isfinite(result.advance)||result.advance<0||result.advance>16382||result.height<1||result.height>256)
        throw std::invalid_argument("Authored text raster bounds: advance="+std::to_string(result.advance)+", height="+std::to_string(result.height));
    result.inkPadding=style.inkPadding;
    result.advance+=effectPad*2;result.height+=effectPad*2;result.baseline+=effectPad;
    const int rasterHeight=result.height+result.inkPadding*2;
    if(rasterHeight>256)throw std::invalid_argument("Authored ink padding exceeds raster bounds");
    auto glyphs=layout.glyphRuns();
    for(auto &run:glyphs){
        const auto original=run.rawFont();
        if(!result.resolvedFamilies.contains(original.familyName()))result.resolvedFamilies.push_back(original.familyName());
        if(raster){
            // Glyph indexes belong to this physical face. A family/style-name
            // lookup can select a different face, invalidating those indexes.
            auto face=original;face.setPixelSize(style.pixelSize);
            if(!face.isValid())throw std::runtime_error("Authored glyph face unavailable");
            run.setRawFont(face);auto positions=run.positions();for(auto &position:positions)position/=precision;run.setPositions(positions);
        }
    }
    // Raster only each bounded tile; neighboring gutters preserve filtering.
    const int width=std::max(1,static_cast<int>(std::ceil(result.advance))+2+result.inkPadding*2);
    const int gutter=1+effectPad,span=2048-2*gutter;
    for(int x=0;x<width;x+=span){
        const int content=std::min(span,width-x),left=std::max(0,x-gutter),right=std::min(width,x+content+gutter);
        QImage pixels(right-left,rasterHeight,QImage::Format_RGBA8888_Premultiplied);pixels.fill(Qt::transparent);
        if(!text.isEmpty()&&raster){
            const auto size=pixels.size();
            pixels=raster(glyphs,size,style.color,QPointF(effectPad+result.inkPadding-left,result.inkPadding+result.baseline-backendBaseline));
            if(pixels.size()!=size||pixels.format()!=QImage::Format_RGBA8888_Premultiplied)
                throw std::runtime_error("Authored native raster contract mismatch");
        }else if(!text.isEmpty()){
            QPainter painter(&pixels);painter.setRenderHint(QPainter::TextAntialiasing);painter.setPen(style.color);
            painter.translate(effectPad+result.inkPadding-left,result.inkPadding+result.baseline-backendBaseline);painter.scale(1.0/precision,1.0/precision);
            layout.draw(&painter,QPointF(0,0));
        }
        if(style.effects.underline||style.effects.strikeout){QPainter painter(&pixels);painter.translate(effectPad+result.inkPadding-left,result.inkPadding+result.baseline-backendBaseline);painter.scale(1.0/precision,1.0/precision);drawTextDecorations(painter,layout,style.effects,style.color);}
        result.tiles.push_back({left-result.inkPadding,x-result.inkPadding,content,engine.prepareRaster(applyTextEffects(std::move(pixels),style.effects))});
    }
    return result;
}
} // namespace playing::visual::normal
