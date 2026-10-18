// SPDX-License-Identifier: MIT
#include "../src/obs/receiver-link.hpp"
#include "playing/companion/profile.hpp"
#include "playing/companion/protected-store.hpp"
#include <QApplication>
#include <QDialog>
#include <QTemporaryDir>
#include <QPushButton>
#include <QPointer>
#include <QEvent>
#include <iostream>
#include <stdexcept>
int main(int argc,char **argv) {
    QApplication app(argc,argv);unsigned checks=0;
    const auto check=[&](bool value,const char *why){++checks;if(!value)throw std::runtime_error(why);};
    using namespace playing::companion;
    try {
        QTemporaryDir temporary;check(temporary.isValid(),"Temporary directory");
        const auto path=temporary.filePath("profile.pcs");
        Profile profile;profile.identity=createIdentity();profile.autoReconnect=true;profile.startup=StartupMode::Receiver;
        profile.receiver=ReceiverSettings{"127.0.0.1",9000,"OBS Playing Companion",certificatePin(profile.identity.certificate),newDeviceCredential()};
        ProtectedStore store(path);store.save(encodeProfile(profile));const auto identity=profile.identity.certificate.toDer();
        int changed=0;QPointer<QDialog> dialog=showPlayingReceiverLink(path,[&]{++changed;},nullptr);
        check(dialog&&dialog->isVisible(),"Receiver dialog visible");
        check(dialog->findChild<QPushButton *>("receiverSearch")&&dialog->findChild<QPushButton *>("receiverPair"),"Pairing controls missing");
        check(changed==0&&decodeProfile(*store.load()).receiver.has_value(),"Dialog mutated link on open");
        auto forget=dialog->findChild<QPushButton *>("receiverForget");check(forget!=nullptr,"Forget control");forget->click();
        const auto saved=decodeProfile(*store.load());
        check(!saved.receiver&&saved.startup==StartupMode::Manual&&!saved.autoReconnect,"Forget retained credentials/startup");
        check(saved.identity.certificate.toDer()==identity,"Forget replaced identity");
        check(changed==1,"Sources not notified of saved change");
        dialog->reject();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        check(!dialog,"Dialog/client not destroyed");
        bool invalid=false;try{showPlayingReceiverLink("relative.pcs",[]{},nullptr);}catch(const std::invalid_argument &){invalid=true;}
        check(invalid,"Relative profile accepted");
        std::cout<<"PASS "<<checks<<" receiver-link GUI/profile checks; no LAN discovery requested\n";return 0;
    }catch(const std::exception &error){std::cerr<<"FAIL "<<checks<<": "<<error.what()<<'\n';return 1;}
}
