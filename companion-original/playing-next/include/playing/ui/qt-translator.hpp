// SPDX-License-Identifier: MIT
#pragma once
#include "i18n.hpp"
#include <QTranslator>
namespace playing::ui {
// Companion owns its application. Never install this into OBS's global Qt UI.
class StandardButtons final:public QTranslator {
public:
    using QTranslator::QTranslator;
    bool isEmpty()const override{return false;}
    QString translate(const char *,const char *source,const char *,int)const override {
        if(!source)return {};
        const auto plain=QString::fromUtf8(source).remove('&');
        const std::pair<const char*,const char*> aliases[]={{"Yes","Sí"},{"No","No"},{"Cancel","Cancelar"},{"OK","Aceptar"},{"Open","Abrir"},{"Save","Guardar"},{"Close","Cerrar"},{"Apply","Aplicar"}};
        for(const auto &alias:aliases)if(plain==QLatin1String(alias.first))return string(alias.second);
        return {};
    }
};
}
