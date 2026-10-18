// SPDX-License-Identifier: MIT
#include "window.hpp"
#include <QCheckBox>
#include <QTimer>
#include <QNetworkInterface>
#include <QSysInfo>
#include <QComboBox>
#include <QDialog>
#include <QFileDialog>
#include <QFormLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QCloseEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QImageReader>
#include <QScrollArea>
#include <QScreen>
#include <QWindow>
#include <QStyle>
#include <QSignalBlocker>
#include <QFileInfo>
#include <cmath>
#include <algorithm>
#include <stdexcept>
namespace playing::companion {
namespace {
QPushButton *button(QLayout *layout,const QString &text,const char *name) {
    auto b=new QPushButton(text); b->setObjectName(name); layout->addWidget(b); return b;
}
bool confirmPair(QWidget *parent,const QString &title,const QString &text) {
    QMessageBox box(QMessageBox::Question,title,text,QMessageBox::Yes|QMessageBox::No,parent);
    box.setTextFormat(Qt::PlainText); box.setDefaultButton(QMessageBox::No);
    return box.exec()==QMessageBox::Yes;
}
QPixmap icon(const QString &kind,int size=64) {
    QPixmap image(size,size); image.fill(Qt::transparent); QPainter p(&image); p.setRenderHint(QPainter::Antialiasing); p.scale(size/64.0,size/64.0);
    const QColor blue("#00acff"); p.setPen(QPen(blue,4)); p.setBrush(Qt::NoBrush);
    if(kind=="wifi") {for(int radius:{26,18,10}) p.drawArc(QRectF(32-radius,38-radius,radius*2,radius*2),40*16,100*16); p.setBrush(blue); p.drawEllipse(QPointF(32,45),4,4);}
    else if(kind=="pc") {p.drawRoundedRect(QRectF(7,9,50,34),2,2); p.drawLine(32,44,32,53); p.drawLine(20,54,44,54);}
    else if(kind=="rekordbox") {p.setPen(QPen(Qt::white,5)); QPainterPath path; for(int i=0;i<6;++i) {const auto angle=i*3.141592653589793/3; const QPointF point(32+24*std::sin(angle),32+24*std::cos(angle)); if(i==0) path.moveTo(point); else path.lineTo(point);} path.closeSubpath(); p.drawPath(path); p.drawEllipse(QPointF(32,32),10,10);}
    else if(kind=="serato") {p.setBrush(Qt::white); p.setPen(Qt::NoPen); p.drawEllipse(QRectF(4,4,56,56)); p.setPen(QPen(QColor("#020b13"),3,Qt::SolidLine,Qt::RoundCap)); const int heights[]={4,11,20,12,23,8,16,5}; for(int i=0;i<8;++i) p.drawLine(12+i*6,32-heights[i],12+i*6,32+heights[i]);}
    else {const auto color=kind=="virtualdj"?QColor("#ff1530"):blue; p.setBrush(color); p.setPen(Qt::NoPen); p.drawEllipse(QRectF(4,4,56,56)); p.setPen(QPen(QColor("#001321"),2)); p.setBrush(Qt::NoBrush); for(int radius:{23,18,13}) p.drawArc(QRectF(32-radius,32-radius,radius*2,radius*2),35*16,245*16); p.setBrush(Qt::white); p.drawEllipse(QPointF(32,32),10,10); p.setBrush(QColor("#001321")); p.drawEllipse(QPointF(32,32),4,4); if(kind!="virtualdj") {p.setPen(QPen(color,6)); p.drawLine(42,23,59,7);}}
    return image;
}
QWidget *card(QVBoxLayout *root,const QString &heading,QVBoxLayout **content) {
    auto frame=new QWidget; frame->setObjectName("card"); auto layout=new QVBoxLayout(frame); layout->setContentsMargins(18,16,18,16); layout->setSpacing(14);
    auto title=new QLabel(heading); title->setObjectName("sectionTitle"); layout->addWidget(title); root->addWidget(frame); *content=layout; return frame;
}
class Bars final : public QWidget {
public: using QWidget::QWidget;
protected:
    void paintEvent(QPaintEvent *) override {QPainter p(this); const int values[]={14,18,29,41,54,46,34,31,30,17,12,10,20,30,40,39,26,24,19,10}; const qreal step=width()/20.0;
        for(int i=0;i<20;++i) {p.fillRect(QRectF(i*step,height()-values[i],std::max(2.0,step-5),values[i]),isEnabled()?QColor(i<10?"#00baff":"#087bc1"):QColor("#17344c"));}}
};
}
Window::Window(QString profilePath,std::string databaseLibrary)
    :store_(std::move(profilePath)),detector_(dj_,sender_),channel_(std::make_shared<providers::network::Channel>()),receiver_(channel_) {
    const auto stored=store_.load();
    if(stored) profile_=decodeProfile(*stored);
    else {Profile first; first.identity=createIdentity(); save(first);}
    receiverView_=profile_.startup==StartupMode::Receiver || (profile_.receiver.has_value()&&profile_.startup!=StartupMode::Sender);
    if(!registerNativeDetectors(dj_,databaseLibrary)) throw std::runtime_error("Native detector unavailable");
    for(const auto &d:profile_.devices) if(!sender_.pair(d.credential)) throw std::runtime_error("Paired device unavailable");
    if(providers::network::registerProvider(remote_,channel_)!=providers::Registration::Registered) throw std::runtime_error("Remote provider unavailable");
    remoteToken_=remote_.acquire({"network",providers::Mode::Automatic,""}).token;
    if(!remoteToken_) throw std::runtime_error("Remote session unavailable");
    setWindowTitle("OBS Playing Companion · Vista previa"); setWindowFlags(Qt::Window|Qt::FramelessWindowHint); resize(550,1180); setObjectName("mainWindow");
    setStyleSheet("QWidget {font-family: 'Bahnschrift'; font-size: 17px; color:#f3f5ff; background:#020d17;} QWidget#card {border:1px solid #12405d; border-radius:16px; background:#03101c;} QLabel {background:transparent; border:0;} QLabel#sectionTitle {color:#91b0db; font-size:23px; font-weight:600;} QPushButton {padding:10px 14px; border:1px solid #426183; border-radius:12px; background:#101e30;} QPushButton:hover {border-color:#00aeff; background:#11283c;} QPushButton:pressed {background:#123955;} QPushButton#providerRow {text-align:left; border-color:transparent; background:transparent;} QPushButton#providerRow[active=true] {border-color:#00b5ff;} QLineEdit,QComboBox,QSpinBox,QListWidget,QPlainTextEdit {padding:7px; background:#061624; border:1px solid #244c69; border-radius:8px;} QTabWidget::pane {border:1px solid #244c69;} QTabBar::tab {background:#11243a; padding:9px;} QCheckBox {spacing:9px;} QScrollArea {border:0;} QScrollBar:vertical {width:7px; background:#061421;} QScrollBar::handle:vertical {background:#264b69; min-height:30px;} QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical {height:0;}");
    auto root=new QVBoxLayout(this); root->setContentsMargins(18,12,18,18); root->setSpacing(16);
    auto header=new QHBoxLayout; auto logo=new QLabel; logo->setPixmap(icon("disc",64)); header->addWidget(logo);
    auto heading=new QLabel("OBS PLAYING<br><span style='color:#00aeff'>COMPANION</span>"); heading->setStyleSheet("font-size:27px; font-weight:700;"); header->addWidget(heading); header->addStretch();
    auto minimize=new QPushButton("—"),close=new QPushButton("×"); for(auto control:{minimize,close}) {control->setFixedSize(40,40); control->setStyleSheet("border:0; background:transparent; font-size:28px; padding:0;"); header->addWidget(control);}
    QObject::connect(minimize,&QPushButton::clicked,this,&QWidget::showMinimized); QObject::connect(close,&QPushButton::clicked,this,&QWidget::close); root->addLayout(header);
    auto scroll=new QScrollArea; scroll->setWidgetResizable(true); root->addWidget(scroll); auto dashboard=new QWidget; auto cards=new QVBoxLayout(dashboard); cards->setContentsMargins(0,0,0,0); cards->setSpacing(17); scroll->setWidget(dashboard);
    QVBoxLayout *songLayout=nullptr; card(cards,"CURRENT TRACK",&songLayout); auto songRow=new QHBoxLayout;
    artwork_=new QLabel; artwork_->setObjectName("artwork"); artwork_->setFixedSize(180,210); artwork_->setAlignment(Qt::AlignCenter); artwork_->setStyleSheet("background:#081827; border:1px solid #364d70; border-radius:13px;"); artwork_->setPixmap(icon("disc",110)); songRow->addWidget(artwork_);
    auto songText=new QVBoxLayout; track_=new QLabel; track_->setObjectName("track"); track_->setWordWrap(true); track_->setStyleSheet("font-size:30px; font-weight:600;"); artist_=new QLabel; artist_->setWordWrap(true); artist_->setStyleSheet("font-size:25px; color:#91b0db;"); songText->addWidget(track_); songText->addWidget(artist_); songText->addStretch(); bars_=new Bars; bars_->setFixedHeight(58); bars_->setToolTip("Indicador decorativo de reproducción; no mide el audio."); songText->addWidget(bars_); songRow->addLayout(songText,1); songLayout->addLayout(songRow);
    QVBoxLayout *netLayout=nullptr; card(cards,"NETWORK",&netLayout); auto netRow=new QHBoxLayout; auto wifi=new QLabel; wifi->setPixmap(icon("wifi",68)); netRow->addWidget(wifi); network_=new QLabel; network_->setObjectName("network"); netRow->addWidget(network_,1); endpoint_=new QLabel; endpoint_->setStyleSheet("font-size:20px;"); netRow->addWidget(endpoint_); netLayout->addLayout(netRow);
    QVBoxLayout *clientsLayout=nullptr; card(cards,"CONNECTED CLIENTS",&clientsLayout); auto titleItem=clientsLayout->takeAt(0); auto clientsHeading=new QHBoxLayout; clientsHeading->addWidget(titleItem->widget()); delete titleItem; clientsHeading->addStretch(); count_=new QLabel; count_->setStyleSheet("color:#00aeff; font-size:23px;"); clientsHeading->addWidget(count_); clientsLayout->insertLayout(0,clientsHeading); auto clientRow=new QHBoxLayout; auto pc=new QLabel; pc->setPixmap(icon("pc",54)); clientRow->addWidget(pc);
    clientsTitle_=qobject_cast<QLabel *>(clientsHeading->itemAt(0)->widget()); clientsTitle_->setObjectName("clientsTitle"); clientsTitle_->setStyleSheet("color:#91b0db; font-size:23px; font-weight:600;");
    devices_=new QListWidget; devices_->setObjectName("devices"); devices_->setFixedHeight(72); clientRow->addWidget(devices_,1);
    linkedDj_=new QLabel; linkedDj_->setObjectName("linkedDj"); linkedDj_->setTextFormat(Qt::PlainText); linkedDj_->setWordWrap(true); linkedDj_->setMinimumHeight(72); clientRow->addWidget(linkedDj_,1); clientsLayout->addLayout(clientRow);
    QVBoxLayout *detectorLayout=nullptr; card(cards,"AUTO DETECTOR",&detectorLayout);
    for(const auto &id:{QString("rekordbox"),QString("virtualdj"),QString("serato")}) {auto row=new QPushButton; row->setObjectName("providerRow"); row->setProperty("providerId",id); row->setMinimumHeight(72); auto rowLayout=new QHBoxLayout(row); rowLayout->setContentsMargins(14,7,14,7); auto logoLabel=new QLabel; logoLabel->setPixmap(icon(id,48)); rowLayout->addWidget(logoLabel); auto nameLabel=new QLabel(id=="rekordbox"?"rekordbox":id=="virtualdj"?"VirtualDJ":"Serato DJ Pro"); nameLabel->setStyleSheet("font-size:22px;"); rowLayout->addWidget(nameLabel); rowLayout->addStretch(); auto indicator=new QLabel("●"); indicator->setObjectName("indicator"); rowLayout->addWidget(indicator); for(auto label:{logoLabel,nameLabel,indicator}) label->setAttribute(Qt::WA_TransparentForMouseEvents); detectorLayout->addWidget(row); providerRows_.push_back(row);}
    auto pairDashboard=button(detectorLayout,"♧   Pair New Device","pairDashboard");
    auto footer=new QHBoxLayout; auto configure=new QPushButton("⚙"); configure->setFixedSize(62,56); configure->setStyleSheet("font-size:30px;"); configure->setToolTip("Configuración y dispositivos"); footer->addWidget(configure); footer->addStretch(); running_=new QLabel; footer->addWidget(running_); root->addLayout(footer);
    auto settingsDialog=new QDialog(this); settingsDialog->setWindowTitle("Configuración del Companion"); settingsDialog->resize(570,670);
    auto settingsRoot=new QVBoxLayout(settingsDialog); auto settingsScroll=new QScrollArea; settingsScroll->setWidgetResizable(true); settingsRoot->addWidget(settingsScroll);
    auto settingsBody=new QWidget; settingsScroll->setWidget(settingsBody); auto settingsLayout=new QVBoxLayout(settingsBody);
    auto tabs=new QTabWidget; settingsLayout->addWidget(tabs);
    auto djPage=new QWidget; auto djLayout=new QVBoxLayout(djPage); auto form=new QFormLayout;
    provider_=new QComboBox; provider_->setObjectName("provider"); provider_->addItems({"Detectar automáticamente","VirtualDJ","rekordbox","Serato","Historial VirtualDJ","Historial rekordbox"}); form->addRow("Programa DJ",provider_);
    history_=new QLineEdit; history_->setObjectName("history"); history_->setPlaceholderText("Vacío: buscar historial automáticamente"); form->addRow("Historial",history_);
    auto browse=button(djLayout,"Elegir historial…","browse");
    QObject::connect(browse,&QPushButton::clicked,this,[this] {const auto path=QFileDialog::getOpenFileName(this,"Elegir historial"); if(!path.isEmpty()) history_->setText(path);});
    address_=new QLineEdit(profile_.sender.address); address_->setObjectName("address"); form->addRow("Dirección del PC DJ",address_);
    port_=new QSpinBox; port_->setObjectName("port"); port_->setRange(1024,65535); port_->setValue(profile_.sender.port); form->addRow("Puerto",port_);
    lan_=new QCheckBox("Permitir conexión desde otro PC de mi red"); lan_->setChecked(profile_.sender.localNetwork); form->addRow(lan_);
    if(profile_.sender.selection) {const auto &r=*profile_.sender.selection; provider_->setCurrentIndex(r.mode==providers::Mode::History?(r.providerId=="virtualdj"?4:5):(r.providerId=="virtualdj"?1:r.providerId=="rekordbox"?2:3)); history_->setText(QString::fromUtf8(r.historyPath)); if(!detector_.select(r)) throw std::runtime_error("Saved selection invalid");}
    preview_=new QCheckBox("Ver canción en este PC"); preview_->setObjectName("preview"); form->addRow(preview_);
    djLayout->insertLayout(0,form);
    autoSend_=new QCheckBox("Iniciar envío automáticamente al abrir"); autoSend_->setObjectName("autoSend"); autoSend_->setChecked(profile_.startup==StartupMode::Sender); djLayout->addWidget(autoSend_);
    QObject::connect(autoSend_,&QCheckBox::toggled,this,[this](bool enabled) {guarded([this,enabled] {setStartup(enabled?StartupMode::Sender:StartupMode::Manual);});});
    auto start=button(djLayout,"Iniciar envío","startSender");
    QObject::connect(start,&QPushButton::clicked,this,[this] {guarded([this] {
        receiverView_=false;
        std::optional<providers::Request> request; const auto index=provider_->currentIndex();
        if(index) request=providers::Request{index==1 || index==4?"virtualdj":index==2 || index==5?"rekordbox":"serato",index>=4?providers::Mode::History:providers::Mode::Automatic,index>=4?history_->text().toUtf8().toStdString():""};
        if(!detector_.select(request)) throw std::runtime_error("Invalid DJ selection");
        Profile next=profile_; next.sender={address_->text().trimmed(),static_cast<quint16>(port_->value()),lan_->isChecked(),request}; save(next);
        // Validate the advertised endpoint before opening a listener.
        pairingCode({address_->text().trimmed(),static_cast<quint16>(port_->value()),"OBS Playing Companion",certificatePin(profile_.identity.certificate),newDeviceCredential()});
        pairingServer_.stop(); receiver_.stop();
        if(!sender_.listen(profile_.identity,lan_->isChecked()?"0.0.0.0":address_->text().trimmed(),static_cast<quint16>(port_->value()))) throw std::runtime_error("Listener unavailable");
        detector_.setPreview(preview_->isChecked()); update();
    });});
    QObject::connect(preview_,&QCheckBox::toggled,this,[this](bool enabled) {guarded([this,enabled] {detector_.setPreview(enabled); update();});});
    auto automaticPair=button(djLayout,"Permitir vinculación durante 2 minutos","automaticPairSender");
    auto pairingStatus=new QLabel; pairingStatus->setWordWrap(true); pairingStatus->setObjectName("pairingStatus"); djLayout->addWidget(pairingStatus);
    pairingServer_.changed=[this,pairingStatus] {pairingStatus->setText(pairingServer_.active()?"Vinculación abierta. Identidad de este PC:\n"+pairingFingerprint(certificatePin(profile_.identity.certificate)):"Vinculación cerrada");};
    QObject::connect(automaticPair,&QPushButton::clicked,this,[this,start] {guarded([this,start] {
        if(!sender_.port() || !lan_->isChecked() || !profile_.sender.localNetwork || address_->text().trimmed()!=profile_.sender.address || port_->value()!=profile_.sender.port) {
            QStringList addresses; for(const auto &interface:QNetworkInterface::allInterfaces()) {
                if(!(interface.flags()&QNetworkInterface::IsUp)||!(interface.flags()&QNetworkInterface::IsRunning)||(interface.flags()&QNetworkInterface::IsLoopBack)) continue;
                for(const auto &entry:interface.addressEntries()) if(entry.ip().protocol()==QAbstractSocket::IPv4Protocol) {
                    const auto ip=entry.ip().toIPv4Address(); if((ip>>24)==10||(ip&0xfff00000)==0xac100000||(ip>>16)==0xc0a8||(ip>>16)==0xa9fe) addresses.append(entry.ip().toString());
                }
            }
            addresses.removeDuplicates(); if(addresses.isEmpty()) throw std::runtime_error("Local network unavailable");
            QString address=addresses.first(); if(addresses.size()>1) {bool ok=false; address=QInputDialog::getItem(this,"Red del PC DJ","Elige la red compartida con el PC de streaming",addresses,0,false,&ok); if(!ok) return;}
            address_->setText(address); lan_->setChecked(true); pairingServer_.stop(); sender_.stop(); start->click();
        }
        if(!sender_.port()) throw std::runtime_error("Start sender first");
        if(!pairingServer_.open(profile_.identity,sender_.port(),QSysInfo::machineHostName().left(80))) throw std::runtime_error("Pairing listener unavailable");
    });});
    QObject::connect(pairDashboard,&QPushButton::clicked,this,[automaticPair] {automaticPair->click();});
    auto closePairWindow=button(djLayout,"Cerrar vinculación","closeAutomaticPair"); QObject::connect(closePairWindow,&QPushButton::clicked,this,[this] {pairingServer_.stop();});
    pairingServer_.requested=[this](QByteArray id,QString deviceName) {QTimer::singleShot(0,this,[this,id,deviceName] {
        if(!pairingServer_.pending(id)) return;
        const auto question=QString("El equipo «%1» solicita vincularse.\n\nComprueba que esta identidad coincide en ambos PCs:\n%2\n\nConfirma solo la solicitud que acabas de iniciar.").arg(deviceName,pairingFingerprint(certificatePin(profile_.identity.certificate)));
        if(!confirmPair(this,"Vincular equipo",question)) {pairingServer_.reject(id); return;}
        if(!pairingServer_.pending(id)) return;
        guarded([this,id,deviceName] {const auto credential=newDeviceCredential(); Profile next=profile_; next.devices.push_back({deviceName,credential}); save(next); if(!sender_.pair(credential)||!pairingServer_.approve(id,credential)) throw std::runtime_error("Pair approval unavailable"); update();});
    });};
    auto pair=button(djLayout,"Vincular mediante código…","pairDevice");
    QObject::connect(pair,&QPushButton::clicked,this,[this] {guarded([this] {
        if(!sender_.port()) throw std::runtime_error("Start sender first");
        bool ok=false; const auto name=QInputDialog::getText(this,"Vincular PC","Nombre del PC",QLineEdit::Normal,"PC de streaming",&ok).trimmed(); if(!ok) return;
        Profile next=profile_; const auto credential=newDeviceCredential(); next.devices.push_back({name,credential});
        const auto code=pairingCode({address_->text().trimmed(),sender_.port(),"OBS Playing Companion",certificatePin(next.identity.certificate),credential});
        save(next); if(!sender_.pair(credential)) throw std::runtime_error("Pairing unavailable"); update();
        QDialog dialog(this); dialog.setWindowTitle("Vincular PC de streaming"); auto layout=new QVBoxLayout(&dialog);
        auto instructions=new QLabel("Introduce este código en el PC de streaming.\nGuárdalo como una contraseña: da acceso a tu canción."); layout->addWidget(instructions);
        auto text=new QPlainTextEdit(code); text->setReadOnly(true); layout->addWidget(text);
        auto close=button(layout,"Cerrar","closePairing"); QObject::connect(close,&QPushButton::clicked,&dialog,&QDialog::accept); dialog.resize(560,300); dialog.exec();
    });});
    auto revoke=button(djLayout,"Revocar vinculación seleccionada","revokeDevice");
    QObject::connect(revoke,&QPushButton::clicked,this,[this] {guarded([this] {
        const auto row=devices_->currentRow(); if(row<0) return; const auto id=profile_.devices.at(static_cast<std::size_t>(row)).credential.id;
        Profile next=profile_; next.devices.erase(next.devices.begin()+row); save(next); sender_.revoke(id); update();
    });});
    tabs->addTab(djPage,"PC DJ");
    auto receiverPage=new QWidget; auto receiverLayout=new QVBoxLayout(receiverPage);
    auto findDj=button(receiverLayout,"Buscar PC DJ y vincular…","findDj");
    auto discoveryDialog=new QDialog(this); discoveryDialog->setWindowTitle("Buscar PC DJ"); discoveryDialog->resize(570,450); auto discoveryLayout=new QVBoxLayout(discoveryDialog);
    auto discoveryHint=new QLabel("En el PC DJ pulsa «Permitir vinculación durante 2 minutos».\nSelecciona el equipo y compara su identidad antes de confirmar."); discoveryHint->setWordWrap(true); discoveryLayout->addWidget(discoveryHint);
    auto available=new QListWidget; available->setObjectName("discoveredDj"); discoveryLayout->addWidget(available);
    auto discoveryStatus=new QLabel; discoveryStatus->setObjectName("discoveryStatus"); discoveryStatus->setWordWrap(true); discoveryLayout->addWidget(discoveryStatus);
    auto searchAgain=button(discoveryLayout,"Buscar de nuevo","searchDj");
    auto selectDj=button(discoveryLayout,"Vincular equipo seleccionado","selectDj");
    auto cancelDiscovery=button(discoveryLayout,"Cerrar","closeDiscovery"); QObject::connect(cancelDiscovery,&QPushButton::clicked,discoveryDialog,&QDialog::reject);
    pairingClient_.changed=[this,available,discoveryStatus] {const auto row=available->currentRow(); available->clear(); for(const auto &device:pairingClient_.devices()) available->addItem(device.name+" · "+device.address); if(row>=0) available->setCurrentRow(row); else if(available->count()==1) available->setCurrentRow(0); discoveryStatus->setText(!pairingClient_.error().isEmpty()?pairingClient_.error():pairingClient_.searching()?"Buscando equipos…":"Búsqueda terminada. Si no aparece, comprueba que la vinculación esté abierta en el PC DJ y ambos estén en la misma red.");};
    const auto search=[this] {guarded([this] {if(!pairingClient_.search()) throw std::runtime_error("Discovery unavailable");});};
    QObject::connect(searchAgain,&QPushButton::clicked,this,search);
    QObject::connect(findDj,&QPushButton::clicked,this,[discoveryDialog,search] {search(); discoveryDialog->exec();});
    QObject::connect(discoveryDialog,&QDialog::finished,this,[this](int) {pairingClient_.stop();});
    QObject::connect(selectDj,&QPushButton::clicked,this,[this,available,discoveryStatus] {guarded([this,available,discoveryStatus] {
        const auto devices=pairingClient_.devices(); const auto row=available->currentRow(); if(row<0||static_cast<std::size_t>(row)>=devices.size()) return; const auto selected=devices[static_cast<std::size_t>(row)];
        if(!confirmPair(this,"Confirmar PC DJ",QString("Vincular con «%1»\n\nComprueba que esta identidad coincide con la que muestra tu PC DJ:\n%2").arg(selected.name,pairingFingerprint(selected.pin)))) return;
        if(!pairingClient_.request(selected,QSysInfo::machineHostName().left(80))) throw std::runtime_error("Pair request unavailable"); discoveryStatus->setText("Confirma la solicitud en el PC DJ…");
    });});
    pairingClient_.completed=[this,discoveryDialog](ReceiverSettings settings) {guarded([this,discoveryDialog,settings] {
        receiverView_=true;
        Profile next=profile_; next.receiver=settings; next.autoReconnect=true; next.startup=StartupMode::Receiver; save(next);
        autoReceive_->setEnabled(true); {const QSignalBlocker sendBlocked(autoSend_),receiveBlocked(autoReceive_),retryBlocked(autoReconnect_); autoSend_->setChecked(false); autoReceive_->setChecked(true); autoReconnect_->setChecked(true);}
        receiver_.setAutoReconnect(true); pairingServer_.stop(); sender_.stop(); detector_.setPreview(false); code_->clear(); discoveryDialog->accept();
        if(!receiver_.connectTo(settings)) throw std::runtime_error("Automatic receiver connect unavailable"); update();
    });};
    autoReceive_=new QCheckBox("Conectar al PC guardado automáticamente al abrir"); autoReceive_->setObjectName("autoReceive"); autoReceive_->setChecked(profile_.startup==StartupMode::Receiver); autoReceive_->setEnabled(profile_.receiver.has_value()); receiverLayout->addWidget(autoReceive_);
    QObject::connect(autoReceive_,&QCheckBox::toggled,this,[this](bool enabled) {guarded([this,enabled] {setStartup(enabled?StartupMode::Receiver:StartupMode::Manual);});});
    autoReconnect_=new QCheckBox("Reconectar automáticamente si se pierde la red"); autoReconnect_->setObjectName("autoReconnect"); autoReconnect_->setChecked(profile_.autoReconnect); receiver_.setAutoReconnect(profile_.autoReconnect); receiverLayout->addWidget(autoReconnect_);
    QObject::connect(autoReconnect_,&QCheckBox::toggled,this,[this](bool enabled) {guarded([this,enabled] {try {Profile next=profile_; next.autoReconnect=enabled; save(next); receiver_.setAutoReconnect(enabled);} catch(...) {const QSignalBlocker blocked(autoReconnect_); autoReconnect_->setChecked(profile_.autoReconnect); throw;}});});
    auto instruction=new QLabel("Pega el código generado en tu PC DJ para vincular este PC."); instruction->setWordWrap(true); receiverLayout->addWidget(instruction);
    code_=new QLineEdit; code_->setEchoMode(QLineEdit::Password); code_->setObjectName("pairingCode"); receiverLayout->addWidget(code_);
    auto connect=button(receiverLayout,"Vincular y conectar","connectReceiver");
    QObject::connect(connect,&QPushButton::clicked,this,[this] {guarded([this] {
        receiverView_=true;
        const auto settings=parsePairingCode(code_->text().trimmed()); Profile next=profile_; next.receiver=settings; save(next); code_->clear(); pairingServer_.stop(); sender_.stop(); detector_.setPreview(false);
        autoReceive_->setEnabled(true);
        if(!receiver_.connectTo(settings)) throw std::runtime_error("Connection unavailable"); update();
    });});
    auto reconnect=button(receiverLayout,"Conectar al PC guardado","reconnectReceiver");
    QObject::connect(reconnect,&QPushButton::clicked,this,[this] {guarded([this] {if(!profile_.receiver) throw std::runtime_error("No paired PC"); receiverView_=true; pairingServer_.stop(); sender_.stop(); detector_.setPreview(false); if(!receiver_.connectTo(*profile_.receiver)) throw std::runtime_error("Connection unavailable"); update();});});
    auto forget=button(receiverLayout,"Olvidar vinculación anterior","forgetReceiver"); forget->setToolTip("Retira el destino de este PC. Para retirar su acceso, revoca también el equipo en el PC DJ.");
    QObject::connect(forget,&QPushButton::clicked,this,[this] {guarded([this] {
        Profile next=profile_; next.receiver.reset(); if(next.startup==StartupMode::Receiver) next.startup=StartupMode::Manual; next.autoReconnect=false; save(next);
        pairingClient_.stop(); receiver_.stop(); code_->clear(); {const QSignalBlocker receiveBlocked(autoReceive_),retryBlocked(autoReconnect_); autoReceive_->setChecked(false); autoReconnect_->setChecked(false);} autoReceive_->setEnabled(false); update();
    });});
    receiverLayout->addStretch(); tabs->addTab(receiverPage,"PC de streaming");
    auto profileImage=button(settingsLayout,"Elegir imagen de perfil del streamer…","profileImage"); QObject::connect(profileImage,&QPushButton::clicked,this,[this] {guarded([this] {const auto path=QFileDialog::getOpenFileName(this,"Imagen de perfil",{},"Imágenes (*.png *.jpg *.jpeg *.webp)"); if(path.isEmpty()) return; Profile next=profile_; next.streamerImage=core::ImageReference{core::ImageKind::LocalFile,path.toUtf8().toStdString(),""}; save(next); displayedImage_.reset(); update();});});
    status_=new QLabel; status_->setObjectName("status"); status_->setWordWrap(true); settingsLayout->addWidget(status_);
    auto stopButton=button(settingsLayout,"Detener conexión","stop"); QObject::connect(stopButton,&QPushButton::clicked,this,[this] {stop();});
    auto done=button(settingsLayout,"Cerrar","closeSettings"); QObject::connect(done,&QPushButton::clicked,settingsDialog,&QDialog::accept);
    QObject::connect(configure,&QPushButton::clicked,settingsDialog,&QDialog::exec);
    for(auto row:providerRows_) QObject::connect(row,&QPushButton::clicked,this,[this,row] {const auto id=row->property("providerId").toString(); provider_->setCurrentIndex(id=="virtualdj"?1:id=="rekordbox"?2:3); guarded([this,id] {if(!detector_.select(providers::Request{id.toStdString(),providers::Mode::Automatic,""})) throw std::runtime_error("Provider selection unavailable"); update();});});
    tabs->setCurrentIndex(receiverView_?1:0);
    QObject::connect(tabs,&QTabWidget::currentChanged,this,[this](int index) {receiverView_=index==1; stop();});
    sender_.clientsChanged=[this] {detector_.syncDemand(); update();}; detector_.changed=[this] {update();};
    receiver_.changed=[this] {remote_.refresh({"network",providers::Mode::Automatic,""}); update();}; update();
    if(profile_.startup!=StartupMode::Manual) QTimer::singleShot(0,this,[this,start,reconnect] {
        if(!startupPending_) return; startupPending_=false;
        if(profile_.startup==StartupMode::Sender) start->click();
        else if(profile_.startup==StartupMode::Receiver) reconnect->click();
    });
}
Window::~Window() {pairingServer_.changed={}; pairingServer_.requested={}; pairingClient_.changed={}; pairingClient_.completed={}; pairingServer_.stop(); pairingClient_.stop(); sender_.clientsChanged={}; detector_.changed={}; receiver_.changed={}; receiver_.stop(); sender_.stop(); detector_.setPreview(false); remote_.release(remoteToken_);}
void Window::save(const Profile &next) {store_.save(encodeProfile(next)); profile_=next;}
void Window::setStartup(StartupMode mode) {
    const auto sync=[this] {const QSignalBlocker sendBlocked(autoSend_),receiveBlocked(autoReceive_),retryBlocked(autoReconnect_); autoSend_->setChecked(profile_.startup==StartupMode::Sender); autoReceive_->setChecked(profile_.startup==StartupMode::Receiver); autoReconnect_->setChecked(profile_.autoReconnect);};
    try {Profile next=profile_; next.startup=mode; if(mode==StartupMode::Receiver) next.autoReconnect=true; save(next);}
    catch(...) {sync(); throw;}
    sync(); receiver_.setAutoReconnect(profile_.autoReconnect);
}
void Window::guarded(const std::function<void()> &action) {
    try {action();} catch(const std::exception &) {QMessageBox::warning(this,"OBS Playing Companion","No se pudo completar la acción. Comprueba la configuración y el perfil protegido.");}
}
void Window::stop() {startupPending_=false; pairingClient_.stop(); pairingServer_.stop(); receiver_.stop(); sender_.stop(); preview_->setChecked(false); detector_.setPreview(false); update();}
void Window::update() {
    const auto selected=devices_->currentRow(); devices_->clear(); const auto clients=sender_.clients();
    for(const auto &d:profile_.devices) {
        const auto active=std::find_if(clients.begin(),clients.end(),[&](const auto &c) {return c.deviceId==d.credential.id;});
        devices_->addItem(d.name+(active==clients.end()?"  ·  Sin conexión":"  ·  "+active->address+"  ·  Connected"));
        devices_->item(devices_->count()-1)->setForeground(QColor(active==clients.end()?"#91b0db":"#14ed79"));
    }
    if(selected>=0 && selected<devices_->count()) devices_->setCurrentRow(selected);
    core::SnapshotPtr snapshot=detector_.snapshot(); QString status="Sin conexión";
    if(receiver_.connected()) {status="Conectado al PC DJ"; snapshot=remote_.snapshot(remoteToken_);}
    else if(receiver_.reconnectPending()) status="Conexión perdida. Reintentando…";
    else if(receiver_.connecting()) status="Conectando con el PC DJ…";
    else if(receiver_.lastFailure()!=ConnectionFailure::None) {
        switch(receiver_.lastFailure()) {
        case ConnectionFailure::CertificatePin: case ConnectionFailure::CertificateValidation: status="La identidad del PC DJ no coincide. Comprueba el emparejamiento."; break;
        case ConnectionFailure::Pairing: status="El PC DJ rechazó el acceso. Vuelve a vincular este PC."; break;
        case ConnectionFailure::Protocol: status="El PC DJ envió datos incompatibles."; break;
        case ConnectionFailure::InvalidSettings: status="Comprueba la dirección, el puerto y el código de vinculación."; break;
        default: status="No se pudo conectar. Comprueba que el PC DJ está encendido y enviando."; break;
        }
    }
    else if(sender_.port()) status=QString("Envío iniciado · %1 PC(s) conectado(s)").arg(sender_.clientCount());
    if(!receiver_.connected() && detector_.status()==Detection::Ambiguous) status+=" · Hay varios programas reproduciendo. Elige uno.";
    status_->setText(status);
    clientsTitle_->setText(receiverView_?"PC DJ VINCULADO":"CONNECTED CLIENTS");
    devices_->setVisible(!receiverView_); count_->setVisible(!receiverView_); linkedDj_->setVisible(receiverView_);
    linkedDj_->setText(profile_.receiver?QString("PC DJ · %1:%2\n%3").arg(profile_.receiver->address).arg(profile_.receiver->port).arg(receiver_.connected()?"Conectado":receiver_.connecting()?"Conectando…":receiver_.reconnectPending()?"Reconectando…":"Guardado · Sin conexión"):"Sin PC DJ vinculado");
    track_->setTextFormat(Qt::PlainText); artist_->setTextFormat(Qt::PlainText);
    track_->setText(snapshot && snapshot->metadata?QString::fromUtf8(snapshot->metadata->title):"Esperando canción");
    artist_->setText(snapshot && snapshot->metadata?QString::fromUtf8(snapshot->metadata->artist):"Sin reproducción activa");
    bars_->setEnabled(snapshot && snapshot->playback==core::Playback::Playing);
    const bool connected=receiver_.connected() || !clients.empty(); network_->setText(connected?"●  Connected":"●  Disconnected"); network_->setStyleSheet(QString("color:%1; font-size:20px;").arg(connected?"#10f080":"#91b0db"));
    const auto address=receiverView_?(profile_.receiver?profile_.receiver->address:QString("—")):address_->text(); const auto port=receiverView_?(profile_.receiver?profile_.receiver->port:quint16(0)):sender_.port();
    endpoint_->setText(QString("<span style='color:#91b0db'>IP</span>　%1<br><span style='color:#91b0db'>Port</span>　%2").arg(address.toHtmlEscaped()).arg(port?QString::number(port):"—"));
    count_->setText(QString::number(clients.size())); const bool running=sender_.port()!=0 || receiver_.connected() || receiver_.connecting() || receiver_.reconnectPending() || preview_->isChecked(); running_->setText(running?"●  Running":"●  Stopped"); running_->setStyleSheet(QString("color:%1; font-size:21px;").arg(running?"#10f080":"#91b0db"));
    const auto candidates=detector_.candidates();
    for(auto row:providerRows_) {const auto id=row->property("providerId").toString().toStdString(); const bool active=std::find(candidates.begin(),candidates.end(),id)!=candidates.end(); row->setProperty("active",active); row->findChild<QLabel *>("indicator")->setStyleSheet(QString("font-size:26px; color:%1;").arg(active?"#11f380":"#446789")); row->style()->unpolish(row); row->style()->polish(row);}
    auto image=core::resolveArtwork(snapshot && snapshot->track?snapshot->track->artwork:core::Artwork{},profile_.streamerImage);
    if(image && image->kind!=core::ImageKind::LocalFile) image=profile_.streamerImage;
    if(!(image==displayedImage_)) {
        displayedImage_=image;
        const auto load=[](const std::optional<core::ImageReference> &ref) {
            if(!ref || ref->kind!=core::ImageKind::LocalFile) return QPixmap{};
            const auto path=QString::fromUtf8(ref->location); const QFileInfo info(path); if(!info.isFile() || info.size()>16*1024*1024) return QPixmap{};
            QImageReader reader(path); const auto size=reader.size(); if(!size.isValid() || size.width()>4096 || size.height()>4096) return QPixmap{};
            reader.setScaledSize(size.scaled(180,210,Qt::KeepAspectRatio)); return QPixmap::fromImage(reader.read());
        };
        auto pixmap=load(image); if(pixmap.isNull()) pixmap=load(profile_.streamerImage);
        artwork_->setPixmap(pixmap.isNull()?icon("disc",110):pixmap);
    }
}
void Window::closeEvent(QCloseEvent *event) {stop(); QWidget::closeEvent(event);}
void Window::mousePressEvent(QMouseEvent *event) {
    if(event->button()==Qt::LeftButton && event->position().y()<95 && windowHandle()) windowHandle()->startSystemMove();
    QWidget::mousePressEvent(event);
}
} // namespace playing::companion
