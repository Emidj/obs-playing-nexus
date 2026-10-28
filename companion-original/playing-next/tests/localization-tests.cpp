// SPDX-License-Identifier: MIT
#include "playing/ui/i18n.hpp"
#include "playing/ui/qt-translator.hpp"
#include "../apps/companion/window.hpp"
#include "../src/obs/receiver-link.hpp"
#include "../src/obs/preset-manager.hpp"
#include "playing/companion/profile.hpp"
#include "playing/companion/protected-store.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QDir>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>
int main(int argc,char **argv){
    QApplication app(argc,argv);app.setQuitOnLastWindowClosed(false);playing::ui::StandardButtons buttons;app.installTranslator(&buttons);
    int checks=0;const auto check=[&](bool ok,const char *why){++checks;if(!ok)throw std::runtime_error(why);};
    try{
        check(argc==3,"Database and output directory required");QDir().mkpath(QString::fromUtf8(argv[2]));QTemporaryDir temporary;
        check(temporary.isValid(),"Temporary profile");
        const char *locales[]={"es-ES","en-US","it_IT","fr-FR","de-DE"};
        const char *expected[]={"Conectar Companion","Connect Companion","Connetti Companion","Connecter Companion","Companion verbinden"};
        for(int i=0;i<5;++i){
            playing::ui::setLanguage(locales[i]);check(QString::fromUtf8(playing::ui::text("Conectar Companion"))==QString::fromUtf8(expected[i]),"Independent language expectation");
            check(QCoreApplication::translate("QPlatformTheme","Cancel")==playing::ui::string("Cancelar"),"Standard dialog button untranslated");
            const auto profilePath=temporary.filePath(QString::number(i)+".pcs");playing::companion::Profile profile;profile.identity=playing::companion::createIdentity();
            playing::companion::ProtectedStore store(profilePath);store.save(playing::companion::encodeProfile(profile));
            playing::companion::Window window(profilePath,argv[1]);window.setAttribute(Qt::WA_DontShowOnScreen);window.show();QApplication::processEvents();
            auto tabs=window.findChild<QTabWidget*>();check(tabs&&tabs->count()==1&&tabs->tabText(0)==playing::ui::string("PC DJ"),"DJ-only tab translation");
            auto pair=window.findChild<QPushButton*>("pairDashboard");check(pair&&pair->text()==playing::ui::string("Conectar con OBS"),"DJ assistant button language");
            check(window.findChild<QLabel*>("track")->text()==playing::ui::string("Esperando canción"),"Waiting message language");
            const QSignalBlocker noListener(window.findChild<QPushButton*>("startSender")),noDiscovery(window.findChild<QPushButton*>("automaticPairSender"));
            pair->click();QApplication::processEvents();auto quick=window.findChild<QDialog*>("quickSetup");check(quick&&quick->isVisible(),"Assistant hidden");
            check(quick->screen()->availableGeometry().contains(quick->frameGeometry()),"Translated DJ assistant exceeds screen");
            auto scroll=quick->findChild<QScrollArea*>();check(scroll&&scroll->horizontalScrollBar()->maximum()==0,"Translated assistant needs horizontal scrolling");
            auto close=quick->findChild<QPushButton*>("quickClose");check(close&&quick->rect().contains(QRect(close->mapTo(quick,QPoint()),close->size())),"Translated close button clipped");
            check(quick->grab().save(QString::fromUtf8(argv[2])+"/companion-"+locales[i]+".png"),"DJ assistant screenshot");quick->close();
            const auto after=playing::companion::decodeProfile(*store.load());check(after.identity.certificate.toDer()==profile.identity.certificate.toDer(),"Language changed pairing identity");
            window.findChild<QPushButton*>("stop")->click();window.close();
            auto receiver=showPlayingReceiverLink(temporary.filePath(QString::number(i)+"-receiver.pcs"),[]{},nullptr,{},false);QApplication::processEvents();
            check(receiver->windowTitle()==QString::fromUtf8(expected[i]),"OBS assistant language");
            check(receiver->screen()->availableGeometry().contains(receiver->frameGeometry()),"Translated OBS assistant exceeds screen");
            receiver->findChild<QPushButton*>("receiverForget")->click();check(receiver->findChild<QLabel*>("receiverStatus")->text()==playing::ui::string("No hay vinculación guardada."),"OBS status language");
            check(receiver->grab().save(QString::fromUtf8(argv[2])+"/receiver-"+locales[i]+".png"),"OBS assistant screenshot");receiver->close();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
            auto manager=playing::presets::showManager(temporary.filePath("library.json"),[]{return QJsonObject{};},[](const playing::presets::Entry&){},nullptr);QApplication::processEvents();
            check(manager->findChild<QPushButton*>("presetApply")->text()==playing::ui::string("Aplicar"),"Preset manager language");manager->close();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        }
        playing::ui::setLanguage("pt-BR");check(QString::fromUtf8(playing::ui::text("Conectar Companion"))=="Connect Companion","Unsupported language must use English");
        playing::ui::setLanguage("DE_at");check(QString::fromUtf8(playing::ui::text("Cerrar"))=="Schließen","Regional/case language matching");
        check(QString::fromUtf8(playing::ui::text("Untranslated brand"))=="Untranslated brand","Unknown brand must stay intact");
        std::cout<<"PASS "<<checks<<" five-language UI checks; no LAN listeners or user profiles\n";return 0;
    }catch(const std::exception &error){std::cerr<<"FAIL "<<checks<<": "<<error.what()<<'\n';return 1;}
}
