// SPDX-License-Identifier: MIT
#include "receiver-link.hpp"
#include "playing/companion/pairing.hpp"
#include "playing/companion/profile.hpp"
#include "playing/companion/protected-store.hpp"
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>
#include <QHostInfo>
QDialog *showPlayingReceiverLink(const QString &path,std::function<void()> changed,QWidget *parent) {
    using namespace playing::companion;
    if(!QFileInfo(path).isAbsolute())throw std::invalid_argument("Receiver profile must be absolute");
    auto dialog=new QDialog(parent);dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setWindowTitle("OBS Playing — vincular PC DJ");
    auto layout=new QVBoxLayout(dialog);
    auto hint=new QLabel("En el Companion del PC DJ permite la vinculación durante 2 minutos.\nBusca tu PC y compara la huella de identidad en ambos equipos.",dialog);
    hint->setWordWrap(true);layout->addWidget(hint);
    auto list=new QListWidget(dialog);layout->addWidget(list);
    auto status=new QLabel(dialog);status->setWordWrap(true);layout->addWidget(status);
    auto search=new QPushButton("Buscar PC DJ",dialog);layout->addWidget(search);
    search->setObjectName("receiverSearch");
    auto pair=new QPushButton("Vincular equipo seleccionado",dialog);layout->addWidget(pair);
    pair->setObjectName("receiverPair");
    auto forget=new QPushButton("Olvidar vinculación anterior",dialog);layout->addWidget(forget);
    forget->setObjectName("receiverForget");
    auto close=new QPushButton("Cerrar",dialog);layout->addWidget(close);
    auto client=new PairingClient(dialog);
    QObject::connect(dialog,&QObject::destroyed,client,[client]{client->changed={};client->completed={};client->stop();});
    client->changed=[client,list,status] {
        const auto selected=list->currentItem()?list->currentItem()->data(Qt::UserRole).toString():QString{};
        list->clear();const auto devices=client->devices();
        for(const auto &device:devices){auto item=new QListWidgetItem(device.name+" — "+device.address,list);item->setData(Qt::UserRole,device.address);if(device.address==selected)list->setCurrentItem(item);}
        status->setText(client->error().isEmpty()?QString("Equipos encontrados: %1").arg(devices.size()):client->error());
    };
    client->completed=[path,changed,dialog,status](ReceiverSettings settings) {
        try {
            if(!QDir().mkpath(QFileInfo(path).absolutePath()))throw std::runtime_error("Profile directory unavailable");
            ProtectedStore store(path);Profile profile;
            if(const auto previous=store.load())profile=decodeProfile(*previous);else profile.identity=createIdentity();
            profile.receiver=std::move(settings);profile.autoReconnect=true;profile.startup=StartupMode::Receiver;
            store.save(encodeProfile(profile));changed();dialog->accept();
        }catch(const std::exception &){status->setText("No se pudo guardar la vinculación. El perfil anterior se conserva.");}
    };
    QObject::connect(search,&QPushButton::clicked,dialog,[client,status]{if(!client->search())status->setText("No se pudo iniciar la búsqueda local.");});
    QObject::connect(pair,&QPushButton::clicked,dialog,[client,list,status,dialog] {
        const auto row=list->currentRow();const auto devices=client->devices();if(row<0||static_cast<std::size_t>(row)>=devices.size())return;
        const auto device=devices.at(static_cast<std::size_t>(row));
        if(QMessageBox::question(dialog,"Confirmar identidad del PC DJ",device.name+"\n"+pairingFingerprint(device.pin)+"\n\n¿Coincide con la huella en el Companion del PC DJ?")!=QMessageBox::Yes)return;
        if(!client->request(device,QHostInfo::localHostName()))status->setText("No se pudo solicitar la vinculación.");
        else status->setText("Confirma este equipo en el Companion del PC DJ.");
    });
    QObject::connect(forget,&QPushButton::clicked,dialog,[path,changed,client,status] {
        try {
            ProtectedStore store(path);const auto bytes=store.load();if(!bytes){status->setText("No hay vinculación guardada.");return;}
            auto profile=decodeProfile(*bytes);profile.receiver.reset();profile.autoReconnect=false;profile.startup=StartupMode::Manual;
            store.save(encodeProfile(profile));client->stop();changed();status->setText("Vinculación local retirada. Puedes revocar también este equipo en el Companion del PC DJ.");
        }catch(const std::exception &){status->setText("No se pudo retirar la vinculación. El perfil anterior se conserva.");}
    });
    QObject::connect(close,&QPushButton::clicked,dialog,&QDialog::reject);
    dialog->resize(580,460);dialog->show();return dialog;
}
