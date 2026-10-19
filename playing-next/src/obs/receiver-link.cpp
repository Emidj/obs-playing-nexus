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
#include <QCheckBox>
#include <QScrollArea>
#include <QScreen>
#include <QTimer>
#include <QSignalBlocker>
#include <QDir>
#include <QFileInfo>
#include <QHostInfo>
#include <algorithm>
QDialog *showPlayingReceiverLink(const QString &path,std::function<void()> changed,QWidget *parent,
    std::function<PlayingReceiverHealth()> health,bool automaticSearch) {
    using namespace playing::companion;
    if(!QFileInfo(path).isAbsolute())throw std::invalid_argument("Receiver profile must be absolute");
    auto dialog=new QDialog(parent);dialog->setObjectName("receiverAssistant");dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle("Conectar Companion");dialog->setSizeGripEnabled(true);
    auto root=new QVBoxLayout(dialog);auto scroll=new QScrollArea;scroll->setWidgetResizable(true);root->addWidget(scroll,1);
    auto body=new QWidget;scroll->setWidget(body);auto layout=new QVBoxLayout(body);layout->setSpacing(10);
    auto hint=new QLabel("En el PC DJ, abre Companion y pulsa «Conectar con OBS».\nLos equipos deben estar en la misma red.");
    hint->setWordWrap(true);hint->setTextFormat(Qt::PlainText);layout->addWidget(hint);
    auto list=new QListWidget;list->setObjectName("receiverDevices");list->setMinimumHeight(100);list->setMaximumHeight(160);layout->addWidget(list);
    auto identity=new QLabel;identity->setObjectName("receiverIdentity");identity->setWordWrap(true);identity->setTextFormat(Qt::PlainText);layout->addWidget(identity);
    auto confirm=new QCheckBox("La identidad coincide con la del Companion");confirm->setObjectName("receiverConfirm");layout->addWidget(confirm);
    auto pair=new QPushButton("Conectar este PC");pair->setObjectName("receiverPair");pair->setEnabled(false);layout->addWidget(pair);
    auto status=new QLabel;status->setObjectName("receiverStatus");status->setWordWrap(true);status->setTextFormat(Qt::PlainText);layout->addWidget(status);
    auto search=new QPushButton("Buscar de nuevo");search->setObjectName("receiverSearch");layout->addWidget(search);
    auto advanced=new QPushButton("Opciones avanzadas");advanced->setCheckable(true);layout->addWidget(advanced);
    auto forget=new QPushButton("Olvidar vinculación anterior");forget->setObjectName("receiverForget");forget->hide();layout->addWidget(forget);
    QObject::connect(advanced,&QPushButton::toggled,forget,&QWidget::setVisible);
    auto close=new QPushButton("Cerrar");root->addWidget(close);QObject::connect(close,&QPushButton::clicked,dialog,&QDialog::reject);
    auto client=new PairingClient(dialog);auto monitor=new QTimer(dialog);monitor->setInterval(350);
    struct View {bool linked=false,pending=false;QString selected;};auto view=std::make_shared<View>();
    const auto checkHealth=[view,status,health]{
        if(!view->linked)return;
        if(!health){status->setText("Vinculación guardada. La reconexión automática está activada.");return;}
        const auto state=health();
        status->setText(QString("Vinculación: guardada\nConexión: %1\nCanción: %2\nImagen en OBS: %3%4")
            .arg(state.connected?"conectada":"esperando al PC DJ",state.track?"recibida":"esperando música",state.image?"disponible":"todavía no disponible",state.message.isEmpty()?QString{}:"\n\n"+state.message));
    };
    const auto showLinked=[view,hint,list,identity,confirm,pair,search,monitor,health,checkHealth]{
        view->linked=true;view->pending=false;list->hide();identity->hide();confirm->hide();pair->hide();
        hint->setText("Tu PC DJ queda guardado. OBS volverá a conectarse automáticamente al abrir esta fuente.");search->setText("Cambiar PC DJ");
        if(health)monitor->start();checkHealth();
    };
    QObject::connect(monitor,&QTimer::timeout,dialog,checkHealth);
    const auto selectedChanged=[client,list,identity,confirm,pair,view]{
        const auto devices=client->devices();const auto row=list->currentRow();
        if(row<0||static_cast<std::size_t>(row)>=devices.size()){identity->clear();pair->setEnabled(false);return;}
        const auto &device=devices[static_cast<std::size_t>(row)];
        const auto key=device.address+":"+QString::number(device.pairingPort)+":"+QString::fromLatin1(device.pin.toHex());
        if(view->selected!=key){view->selected=key;confirm->setChecked(false);}
        identity->setText("Comprueba esta identidad en el Companion:\n"+pairingFingerprint(device.pin));
        pair->setEnabled(confirm->isChecked()&&!view->pending);
    };
    QObject::connect(list,&QListWidget::currentRowChanged,dialog,[selectedChanged]{selectedChanged();});
    QObject::connect(confirm,&QCheckBox::toggled,dialog,[selectedChanged]{selectedChanged();});
    client->changed=[client,list,status,view,selectedChanged] {
        if(view->linked)return;
        {const QSignalBlocker block(list);list->clear();const auto devices=client->devices();
         for(const auto &device:devices){auto item=new QListWidgetItem(device.name+" — "+device.address,list);
            const auto key=device.address+":"+QString::number(device.pairingPort)+":"+QString::fromLatin1(device.pin.toHex());item->setData(Qt::UserRole,key);if(key==view->selected)list->setCurrentItem(item);}
         if(list->currentRow()<0&&devices.size()==1)list->setCurrentRow(0);}
        if(!client->error().isEmpty())view->pending=false;
        selectedChanged();
        if(view->pending)return;
        status->setText(!client->error().isEmpty()?"No se pudo completar la conexión. Vuelve a abrir la vinculación en el PC DJ y pulsa Buscar de nuevo.":client->searching()?"Buscando tu PC DJ…":list->count()?"Selecciona tu PC DJ y confirma su identidad.":"No se encuentra el PC DJ. Comprueba la misma red y el permiso de red del Companion en Windows.");
    };
    client->completed=[path,changed,status,client,showLinked](ReceiverSettings settings) {
        try {
            if(!QDir().mkpath(QFileInfo(path).absolutePath()))throw std::runtime_error("Profile directory unavailable");
            ProtectedStore store(path);Profile profile;
            if(const auto previous=store.load())profile=decodeProfile(*previous);else profile.identity=createIdentity();
            profile.receiver=std::move(settings);profile.autoReconnect=true;profile.startup=StartupMode::Receiver;
            store.save(encodeProfile(profile));changed();showLinked();QTimer::singleShot(0,client,[client]{client->stop();});
        }catch(const std::exception &){status->setText("No se pudo guardar la vinculación. El perfil anterior se conserva.");}
    };
    const auto startSearch=[client,status,view,list,identity,confirm,pair,search,hint,monitor] {
        view->linked=false;view->pending=false;monitor->stop();confirm->setChecked(false);list->show();identity->show();confirm->show();pair->show();search->setText("Buscar de nuevo");
        hint->setText("En el PC DJ pulsa «Conectar con OBS». Elige aquí tu equipo y confirma que su identidad coincide.");
        if(!client->search())status->setText("No se pudo iniciar la búsqueda. Comprueba la conexión de red de este PC.");
    };
    QObject::connect(search,&QPushButton::clicked,dialog,startSearch);
    QObject::connect(pair,&QPushButton::clicked,dialog,[client,list,status,view,confirm,pair] {
        const auto devices=client->devices();const auto row=list->currentRow();
        if(!confirm->isChecked()||view->pending||row<0||static_cast<std::size_t>(row)>=devices.size())return;
        const auto selected=devices.at(static_cast<std::size_t>(row));
        const auto key=selected.address+":"+QString::number(selected.pairingPort)+":"+QString::fromLatin1(selected.pin.toHex());if(key!=view->selected)return;
        view->pending=true;pair->setEnabled(false);
        if(!client->request(selected,QHostInfo::localHostName())){view->pending=false;pair->setEnabled(true);status->setText("No se pudo solicitar la conexión. Pulsa Buscar de nuevo.");}
        else status->setText("Acepta la solicitud en el Companion del PC DJ. No necesitas copiar ningún código.");
    });
    QObject::connect(forget,&QPushButton::clicked,dialog,[path,changed,client,status,view,monitor] {
        try {
            ProtectedStore store(path);const auto bytes=store.load();if(!bytes){status->setText("No hay vinculación guardada.");return;}
            auto profile=decodeProfile(*bytes);profile.receiver.reset();profile.autoReconnect=false;profile.startup=StartupMode::Manual;
            store.save(encodeProfile(profile));view->linked=false;view->pending=false;monitor->stop();client->stop();changed();status->setText("Vinculación retirada. Pulsa Cambiar PC DJ para elegir otro equipo.");
        }catch(const std::exception &){status->setText("No se pudo retirar la vinculación. El perfil anterior se conserva.");}
    });
    QObject::connect(dialog,&QDialog::finished,client,[client,monitor]{monitor->stop();client->changed={};client->completed={};client->stop();});
    try {const auto previous=ProtectedStore(path).load();if(previous&&decodeProfile(*previous).receiver)showLinked();}
    catch(const std::exception &){status->setText("No se puede leer la vinculación guardada. Su archivo se conserva.");}
    if(!view->linked&&automaticSearch)QTimer::singleShot(0,dialog,startSearch);
    const auto available=dialog->screen()->availableGeometry();dialog->resize(std::min(480,available.width()-24),std::min(570,available.height()-64));
    dialog->show();return dialog;
}
