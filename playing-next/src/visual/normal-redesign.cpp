// SPDX-License-Identifier: GPL-2.0-or-later
// Native art direction from the preserved NORMAL screenshots. These recipes
// prepare bounded vector rasters on the GUI owner; all GPU work stays shared.
#include "playing/visual/authored-text.hpp"
#include "playing/visual/preset-definition.hpp"
#include <QPainter>
#include <QLinearGradient>
#include <QRadialGradient>
#include <algorithm>
#include <cmath>
namespace playing::visual::normal {
PresetDefinition customizedDefinition(const PresetDefinition &base,const TextOverrides &overrides){
    auto result=base;
    for(std::size_t i=0;i<overrides.size();++i){
        const auto &value=overrides[i];auto &row=result.text[i+1];
        value.effects.validate();
        if(value.family){const auto family=value.family->trimmed();if(family.isEmpty()||family.size()>128)throw std::invalid_argument("Invalid NORMAL font family");for(const auto c:family)if(c.category()==QChar::Other_Control||c.category()==QChar::Other_Format)throw std::invalid_argument("Invalid NORMAL font family");row.family=family;}
        if(value.pixelSize){if(*value.pixelSize<8||*value.pixelSize>128)throw std::invalid_argument("Invalid NORMAL font size");row.pixelSize=*value.pixelSize;}
        if(value.bold)row.weight=*value.bold?QFont::Bold:QFont::Normal;
        if(value.italic)row.italic=*value.italic;
        if(value.color){if(!value.color->isValid())throw std::invalid_argument("Invalid NORMAL text color");row.color=*value.color;}
    }
    return result;
}
NativeLayout redesignedLayout(NativePreset preset,int width,const std::array<QSize,3>&metrics,int gap) {
    if(!redesigned(preset))throw std::invalid_argument("Not a redesigned NORMAL");
    return redesignedDefinitionLayout(presetDefinition(preset),width,metrics,gap);
}
NativeLayout redesignedDefinitionLayout(const PresetDefinition &style,int width,const std::array<QSize,3>&metrics,int gap){
    if(!style.nativeRecipe)throw std::invalid_argument("Not a native recipe definition");
    if(width<400||width>1600||gap<0||gap>80)throw std::invalid_argument("Redesign layout bounds");
    NativeLayout layout;const double scale=width/500.0;double y=style.top*scale;
    for(std::size_t i=0;i<metrics.size();++i){
        if(metrics[i].width()<0||metrics[i].width()>1048576||metrics[i].height()<0||metrics[i].height()>1024)throw std::invalid_argument("Redesign text bounds");
        if(i)y+=metrics[i-1].height()+gap+style.rowGap*scale;
        const double available=std::floor(style.available*scale),clipped=std::min<double>(metrics[i].width(),available);
        layout.rows[i].box={style.center?(width-clipped)/2:style.left*scale,y,clipped,static_cast<double>(metrics[i].height())};
        layout.rows[i].overflow=std::max(0,metrics[i].width()-static_cast<int>(available));
    }
    layout.canvas={width,static_cast<int>(std::ceil(layout.rows[2].box.bottom()+style.bottom*scale))};
    if(layout.canvas.height()>2048)throw std::invalid_argument("Redesigned canvas exceeds native bounds");
    return layout;
}
PreparedNormalPtr prepareRedesignedNormal(oto::visual::VisualEngine &engine,NativePreset preset,
    TrackPtr track,int width,const QString &label,int gap,const TextOverrides &overrides) {
    if(!redesigned(preset))throw std::invalid_argument("Not a redesigned NORMAL");
    return prepareRedesignedDefinition(engine,presetDefinition(preset),std::move(track),width,label,gap,overrides);
}
PreparedNormalPtr prepareRedesignedDefinition(oto::visual::VisualEngine &engine,const PresetDefinition &base,
    TrackPtr track,int width,const QString &label,int gap,const TextOverrides &overrides){
    const auto style=customizedDefinition(base,overrides);
    if(!style.nativeRecipe)throw std::invalid_argument("Not a native recipe definition");
    if(!track||!core::validMetadata(*track))throw std::invalid_argument("Invalid native redesigned track");
    const double scale=width/500.0;
    if(width<400||width>1600||gap<0||gap>80)throw std::invalid_argument("Native redesign bounds");
    auto result=std::make_shared<PreparedNormal>();result->track=std::move(track);result->legacyTextBlend=false;
    const std::array<QString,3> text{label,QString::fromUtf8(result->track->title.c_str()),QString::fromUtf8(result->track->artist.c_str())};
    std::array<QSize,3> sizes;
    for(std::size_t i=0;i<3;++i){
        const auto &definition=style.text[i];
        AuthoredTextStyle font;font.family=definition.family;font.boundedEllipsis=true;
        font.pixelSize=std::clamp(static_cast<int>(std::round(definition.pixelSize*scale)),8,128);
        if(i){const auto &override=overrides[i-1];if(override.pixelSize)font.pixelSize=*override.pixelSize;font.effects=override.effects;}
        font.weight=definition.weight;font.italic=definition.italic;font.color=definition.color;
        font.fallbackFamilies={QStringLiteral("Microsoft YaHei"),QStringLiteral("Segoe UI Emoji")};
        // Empty metadata retains a line box so panel geometry remains stable.
        auto layer=prepareAuthoredText(engine,text[i].isEmpty()?QString(QChar(0xa0)):(definition.uppercase?text[i].toUpper():text[i]),font);
        sizes[i]={static_cast<int>(std::ceil(layer.advance)),layer.height};result->text[i]=std::move(layer.tiles);
    }
    result->layout=redesignedDefinitionLayout(style,width,sizes,gap);
    for(int pass=0;pass<2;++pass){
        QImage pixels(result->layout.canvas,QImage::Format_RGBA8888_Premultiplied);pixels.fill(Qt::transparent);
        QPainter painter(&pixels);paintPresetArt(painter,style.art[static_cast<std::size_t>(pass)],result->layout);painter.end();
        auto frame=engine.prepareRaster(std::move(pixels));
        if(pass==0)result->background=std::move(frame);
        else {PaintLayer layer;layer.pixels=std::move(frame);layer.travel=style.travel*scale;
            layer.period=style.period;layer.pulse=style.pulse;
            result->ornaments.push_back(std::move(layer));}
    }
    result->animatedDecoration=true;return result;
}
} // namespace playing::visual::normal
