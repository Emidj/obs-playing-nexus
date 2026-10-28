// SPDX-License-Identifier: MIT
#pragma once
#include <QRectF>
#include <QSize>
#include <algorithm>
namespace playing::visual {
// Apertures measured in the preserved reference assets, rather than inferred
// from canvas proportions. Both smaller covers and text keep the same centre.
inline QRectF verticalVinylCoverFrame(QSize canvas) {
    return {canvas.width()*.17,canvas.height()*.22,canvas.width()*.66,canvas.width()*.66};
}
inline QRectF verticalCoverAperture(int style,QSize canvas) {
    const double w=canvas.width(),h=canvas.height();
    if(style==10)return {w*309/1035,h*412/1519,w*441/1035,h*439/1519};
    if(style==11)return {w*246/938,h*408/1677,w*462/938,h*474/1677};
    if(style==12){
        const auto frame=verticalVinylCoverFrame(canvas);
        // Same integer crop as prepareNeonClub for the 2027x776 vinyl asset.
        const int x=int(2027*.065),y=int(776*.17),cw=int(2027*.242),ch=int(776*.63);
        return {frame.x()+frame.width()*(159-x)/cw,frame.y()+frame.height()*(163-y)/ch,
            frame.width()*441/cw,frame.height()*438/ch};
    }
    return {};
}
inline QRect centredVerticalCover(int style,QSize canvas,int requested) {
    const auto aperture=verticalCoverAperture(style,canvas);
    const int size=std::max(1,std::min(requested,int(std::min(aperture.width(),aperture.height()))));
    return {qRound(aperture.center().x()-size/2.),qRound(aperture.center().y()-size/2.),size,size};
}
}
