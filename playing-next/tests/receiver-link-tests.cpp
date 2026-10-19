#include "playing/ui/i18n.hpp"
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
#include <QLabel>
#include <QCheckBox>
#include <QListWidget>
#include <QElapsedTimer>
#include <QThread>
#include <QSslSocket>
#include "playing/companion/pairing.hpp"
#include <iostream>
#include <stdexcept>
int main(int argc,char **argv) {playing::ui::setLanguage("es");
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
        // A saved link is never presented as an authenticated connection.
        store.save(encodeProfile(profile));PlayingReceiverHealth health;
        dialog=showPlayingReceiverLink(path,[]{},nullptr,[&]{return health;},false);
        auto status=dialog->findChild<QLabel *>("receiverStatus");check(status&&status->text().contains("esperando al PC DJ"),"Saved profile falsely shown connected");
        health.connected=true;
        const auto wait=[&](const std::function<bool()> &ready){QElapsedTimer clock;clock.start();while(!ready()&&clock.elapsed()<5000){QApplication::processEvents();QThread::msleep(1);}return ready();};
        check(wait([&]{return status->text().contains("Conexión: conectada");}),"Live connection status missing");
        check(status->text().contains("Canción: esperando música"),"Connection invented a song");
        health.track=true;health.image=true;
        check(wait([&]{return status->text().contains("Canción: recibida")&&status->text().contains("Imagen en OBS: disponible");}),"Song/image status not independent");
        dialog->reject();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);check(!dialog,"Live status dialog retained");
        // Exercise the actual UI confirmation, discovery, TLS pairing and protected
        // profile using loopback only; no local-network announcement or user profile.
        check(QSslSocket::setActiveBackend("schannel"),"Schannel");
        Sender sender;const auto localIdentity=createIdentity();check(sender.listen(localIdentity,"127.0.0.1",0),"Loopback sender");
        PairingServer server;check(server.open(localIdentity,sender.port(),"Fixture DJ",QHostAddress::LocalHost,0),"Loopback pairing window");
        server.requested=[&](QByteArray request,QString){const auto credential=newDeviceCredential();check(sender.pair(credential),"Register confirmed device");check(server.approve(request,credential),"Fixture approval");};
        const auto newPath=temporary.filePath("new.pcs");auto channel=std::make_shared<playing::providers::network::Channel>();Receiver receiver(channel);
        dialog=showPlayingReceiverLink(newPath,[&]{const auto saved=decodeProfile(*ProtectedStore(newPath).load());check(saved.receiver&&saved.autoReconnect&&saved.startup==StartupMode::Receiver,"Automatic receiver settings");check(receiver.connectTo(*saved.receiver),"Connect saved receiver");},nullptr,[&]{PlayingReceiverHealth h;h.connected=receiver.connected();return h;},false);
        PairingClient *client=nullptr;for(auto child:dialog->children())if(auto candidate=dynamic_cast<PairingClient *>(child))client=candidate;
        check(client&&client->search(QHostAddress::LocalHost,server.discoverySocketPort()),"Start explicit loopback discovery");
        auto list=dialog->findChild<QListWidget *>("receiverDevices");auto confirm=dialog->findChild<QCheckBox *>("receiverConfirm");auto pair=dialog->findChild<QPushButton *>("receiverPair");
        check(wait([&]{return list->count()==1;}),"Single discovered DJ");check(list->currentRow()==0,"Single PC not selected automatically");
        check(!pair->isEnabled(),"Pairing bypassed identity confirmation");confirm->setChecked(true);check(pair->isEnabled(),"Explicit identity confirmation ignored");pair->click();
        check(wait([&]{return receiver.connected();}),"UI pairing did not connect TLS");
        auto live=dialog->findChild<QLabel *>("receiverStatus");check(wait([&]{return live->text().contains("Conexión: conectada");}),"Pairing window hid connection result");
        check(dialog->isVisible(),"Assistant closed before showing actual status");
        dialog->reject();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);check(!dialog,"Pairing dialog retained");receiver.stop();server.stop();sender.stop();
        std::cout<<"PASS "<<checks<<" receiver-link GUI/profile checks; explicit loopback discovery, identity confirmation, TLS and live state\n";return 0;
    }catch(const std::exception &error){std::cerr<<"FAIL "<<checks<<": "<<error.what()<<'\n';return 1;}
}
