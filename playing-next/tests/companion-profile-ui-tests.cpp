#include "playing/ui/i18n.hpp"
// SPDX-License-Identifier: MIT
#include "window.hpp"
#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QTemporaryDir>
#include <QTcpServer>
#include <QThread>
#include <QSslSocket>
#include <QMessageBox>
#include <QTimer>
#include <QInputDialog>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QScreen>
#include <QTabWidget>
#include <QSignalBlocker>
#include <iostream>
#include <stdexcept>
using namespace playing::companion;
namespace {
unsigned checks=0;
unsigned unexpectedDialogs=0;
void check(bool value,const char *reason) {++checks; if(!value) throw std::runtime_error(reason);}
template<class F> bool fails(F f) {try {f(); return false;} catch(const std::exception &) {return true;}}
bool wait(const std::function<bool()> &condition) {QElapsedTimer t; t.start(); while(!condition() && t.elapsed()<5000) {QApplication::processEvents(); QThread::msleep(1);} return condition();}
template<class T> T *child(Window &w,const char *name) {auto value=w.findChild<T *>(name); if(!value) throw std::runtime_error("UI control missing"); return value;}
QByteArray read(const QString &path) {QFile f(path); if(!f.open(QIODevice::ReadOnly)) throw std::runtime_error("Read fixture"); return f.readAll();}
void write(const QString &path,const QByteArray &bytes) {QFile f(path); if(!f.open(QIODevice::WriteOnly) || f.write(bytes)!=bytes.size()) throw std::runtime_error("Write fixture");}
Profile profile() {
    Profile p; p.identity=createIdentity(); p.devices.push_back({"STREAM-PC",newDeviceCredential()});
    p.receiver=ReceiverSettings{"127.0.0.1",9000,"OBS Playing Companion",certificatePin(p.identity.certificate),p.devices[0].credential}; return p;
}
void codec(const Profile &p) {
    check(!p.identity.certificate.isNull() && p.identity.privateKey.length()==2048,"Native CNG identity");
    const auto encoded=encodeProfile(p); const auto restored=decodeProfile(encoded);
    check(certificatePin(p.identity.certificate)==certificatePin(restored.identity.certificate) && p.identity.privateKey.toDer()==restored.identity.privateKey.toDer(),"Identity roundtrip");
    check(restored.devices.size()==1 && restored.devices[0].credential.secret==p.devices[0].credential.secret,"Devices roundtrip");
    check(restored.receiver && restored.receiver->device.secret==p.receiver->device.secret,"Receiver roundtrip");
    check(restored.sender.port==9000 && !restored.sender.localNetwork && !restored.sender.selection,"Sender preferences roundtrip");
    const auto code=pairingCode(*p.receiver); const auto paired=parsePairingCode(code);
    check(paired.address==p.receiver->address && paired.port==9000 && paired.device.secret==p.receiver->device.secret && paired.certificateSha256==p.receiver->certificateSha256,"Pairing roundtrip");
    check(fails([&] {parsePairingCode("BAD"+code);}),"Pair prefix");
    check(fails([&] {parsePairingCode(code+"=");}),"Noncanonical pair");
    check(fails([&] {parsePairingCode(QString(4097,'x'));}),"Pair bound");
    auto settings=*p.receiver; settings.address="https://example.com"; check(fails([&] {pairingCode(settings);}),"URL as endpoint");
    settings=*p.receiver; settings.address="0.0.0.0"; check(fails([&] {pairingCode(settings);}),"Wildcard endpoint");
    settings=*p.receiver; settings.certificateSha256.clear(); check(fails([&] {pairingCode(settings);}),"Missing trust pin");
    settings=*p.receiver; settings.port=0; check(fails([&] {pairingCode(settings);}),"Zero port");
    auto bad=p; bad.devices.push_back(p.devices[0]); check(fails([&] {encodeProfile(bad);}),"Duplicate device");
    bad=p; bad.devices[0].name=QString(81,'x'); check(fails([&] {encodeProfile(bad);}),"Name limit");
    bad=p; bad.devices[0].credential.secret="short"; check(fails([&] {encodeProfile(bad);}),"Short secret");
    bad=p; bad.identity={}; check(fails([&] {encodeProfile(bad);}),"Missing identity");
    bad=p; bad.streamerImage=playing::core::ImageReference{playing::core::ImageKind::Https,"https://example.com/profile.png",""}; check(fails([&] {encodeProfile(bad);}),"Remote profile image in local preview config");
    bad=p; bad.sender.address="192.168.1.21"; check(fails([&] {encodeProfile(bad);}),"Remote endpoint advertised for local listener");
    auto object=QJsonDocument::fromJson(encoded).object(); object["version"]=4; check(fails([&] {decodeProfile(QJsonDocument(object).toJson());}),"Unknown version");
    object=QJsonDocument::fromJson(encoded).object(); object["version"]=1; object.remove("autoReconnect"); object.remove("startup"); const auto old=decodeProfile(QJsonDocument(object).toJson()); check(!old.autoReconnect && old.startup==StartupMode::Manual && old.devices[0].credential.secret==p.devices[0].credential.secret && old.identity.privateKey.toDer()==p.identity.privateKey.toDer(),"Version 1 migration preserved credentials and identity");
    object=QJsonDocument::fromJson(encoded).object(); object["version"]=2; object.remove("startup"); check(decodeProfile(QJsonDocument(object).toJson()).startup==StartupMode::Manual,"Version 2 remains manual");
    bad=p; bad.startup=StartupMode::Receiver; check(decodeProfile(encodeProfile(bad)).startup==StartupMode::Receiver,"Receiver startup roundtrip"); bad.receiver.reset(); check(fails([&] {encodeProfile(bad);}),"Automatic receiver without paired PC");
    object=QJsonDocument::fromJson(encoded).object(); object["startup"]="both"; check(fails([&] {decodeProfile(QJsonDocument(object).toJson());}),"Invalid startup role");
    object=QJsonDocument::fromJson(encoded).object(); object["extra"]=1; check(fails([&] {decodeProfile(QJsonDocument(object).toJson());}),"Unknown profile field");
    check(fails([&] {decodeProfile(encoded.first(20));}),"Truncated profile");
    check(fails([&] {decodeProfile(QByteArray(65537,'x'));}),"Profile limit");
    QTemporaryDir dir; ProtectedStore store(dir.filePath("profile.pcs")); store.save(encoded);
    check(!read(dir.filePath("profile.pcs")).contains("PRIVATE KEY") && !read(dir.filePath("profile.pcs")).contains(p.devices[0].credential.secret.toBase64()),"Profile secrets on disk");
    check(decodeProfile(*store.load()).identity.privateKey.toDer()==p.identity.privateKey.toDer(),"Protected identity restart");
}
void automatic(const QString &db) {
    QTemporaryDir dir; check(dir.isValid(),"Automatic startup fixture");
    QTcpServer reserve; check(reserve.listen(QHostAddress::LocalHost,0),"Automatic startup port"); const auto port=reserve.serverPort(); reserve.close();
    const auto history=dir.filePath("history.txt"); write(history,QByteArray::fromStdString(u8"22:03 : Björk - Jóga\r\n"));
    auto dj=profile(); dj.receiver.reset(); dj.sender.port=port; dj.sender.selection=playing::providers::Request{"virtualdj",playing::providers::Mode::History,history.toUtf8().toStdString()}; dj.startup=StartupMode::Sender;
    Profile rx; rx.identity=createIdentity(); rx.startup=StartupMode::Receiver; rx.autoReconnect=true;
    rx.receiver=ReceiverSettings{"127.0.0.1",port,"OBS Playing Companion",certificatePin(dj.identity.certificate),dj.devices[0].credential};
    const auto djPath=dir.filePath("dj.pcs"),rxPath=dir.filePath("rx.pcs"); ProtectedStore djStore(djPath),rxStore(rxPath); djStore.save(encodeProfile(dj)); rxStore.save(encodeProfile(rx));
    {
        Window sender(djPath,db.toUtf8().toStdString()),receiver(rxPath,db.toUtf8().toStdString());
        check(wait([&] {return child<QLabel>(receiver,"track")->text()==QString::fromUtf8(u8"Jóga");}),"Automatic startup without button/code input");
        check(child<QLabel>(sender,"network")->text().contains(QString::fromUtf8(playing::ui::text("Conectado"))),"Automatic authenticated client");
        child<QPushButton>(receiver,"stop")->click(); check(child<QLabel>(receiver,"track")->text()=="Esperando canción","Automatic session stop clears song");
        child<QCheckBox>(receiver,"autoSend")->setChecked(true); check(decodeProfile(*rxStore.load()).startup==StartupMode::Sender && !child<QCheckBox>(receiver,"autoReceive")->isChecked(),"Automatic startup roles are exclusive");
        child<QCheckBox>(receiver,"autoReceive")->setChecked(true); check(decodeProfile(*rxStore.load()).startup==StartupMode::Receiver && !child<QCheckBox>(receiver,"autoSend")->isChecked(),"Automatic receiver preference persisted");
        child<QCheckBox>(receiver,"autoReceive")->setChecked(false); check(decodeProfile(*rxStore.load()).startup==StartupMode::Manual,"Disable automatic startup persists");
        receiver.close(); sender.close();
    }
    {
        Window cancelled(djPath,db.toUtf8().toStdString()); child<QPushButton>(cancelled,"stop")->click(); QApplication::processEvents();
        check(reserve.listen(QHostAddress::LocalHost,port),"Stop before startup event opened listener"); reserve.close();
    }
    rxStore.save(encodeProfile(rx));
    {
        Window offline(rxPath,db.toUtf8().toStdString());
        check(wait([&] {return child<QLabel>(offline,"status")->text().contains("Reintentando");}),"Automatic receiver retries when DJ starts later");
        child<QPushButton>(offline,"stop")->click(); check(child<QLabel>(offline,"status")->text()=="Sin conexión","Automatic retry cancelled by Stop");
    }
}
void selectionPreview(const QString &db) {
    QTemporaryDir dir; check(dir.isValid(),"Preview selection fixture");
    const auto path=dir.filePath("profile.pcs"),history=dir.filePath("history.txt");
    auto p=profile(); p.receiver.reset(); ProtectedStore store(path); store.save(encodeProfile(p));
    write(history,"22:10 : Local Artist - Preview without sender\r\n");
    Window window(path,db.toUtf8().toStdString());
    auto tabs=window.findChild<QTabWidget *>();check(tabs&&tabs->count()==1&&tabs->tabText(0)==playing::ui::string("PC DJ"),"Streaming PC option remains in Companion UI");
    child<QComboBox>(window,"provider")->setCurrentIndex(3);
    const auto saved=decodeProfile(*store.load());
    check(saved.sender.selection && saved.sender.selection->providerId=="serato","DJ selector waits for Start Sender");
    check(child<QLabel>(window,"detectorStatus")->text().contains("Ver canción"),"Serato discovery setup hint missing");
    child<QLineEdit>(window,"history")->setText(history);
    child<QComboBox>(window,"provider")->setCurrentIndex(4);
    child<QCheckBox>(window,"preview")->setChecked(true);
    check(wait([&] {return child<QLabel>(window,"track")->text()=="Preview without sender";}),"Local preview ignored selected DJ before Start Sender");
    check(child<QLabel>(window,"network")->text().contains(playing::ui::string("●  Disconnected")),"Local preview opened sender listener");
    child<QCheckBox>(window,"preview")->setChecked(false);
    check(child<QLabel>(window,"track")->text()=="Esperando canción","Local preview stop retained active track");
    {const QSignalBlocker startBlocked(child<QPushButton>(window,"startSender")),pairBlocked(child<QPushButton>(window,"automaticPairSender"));
     child<QPushButton>(window,"pairDashboard")->click();QApplication::processEvents();
     auto quick=child<QDialog>(window,"quickSetup");check(quick->isVisible(),"One-button assistant missing");
     const auto capture=QDir(QCoreApplication::applicationDirPath()).filePath("quick-assistant.png");check(quick->grab().save(capture),"Assistant capture failed");
     check(child<QCheckBox>(window,"preview")->isChecked(),"Assistant requires manual local preview");
     check(quick->findChildren<QLineEdit *>().isEmpty(),"Assistant asks for IP or codes");
     check(quick->screen()->availableGeometry().contains(quick->frameGeometry()),"Assistant exceeds available screen");quick->reject();}
    child<QPushButton>(window,"stop")->click();
    {const auto fresh=dir.filePath("fresh.pcs");Window first(fresh,db.toUtf8().toStdString());
     check(child<QCheckBox>(first,"autoSend")->isChecked()&&child<QCheckBox>(first,"preview")->isChecked(),"Fresh DJ Companion requires automatic-start/preview checkboxes");
     check(decodeProfile(*ProtectedStore(fresh).load()).startup==StartupMode::Sender,"Fresh DJ startup not saved");child<QPushButton>(first,"stop")->click();}
}
void gui(Profile p,const QString &db,const QString &output,const QString &image) {
    QTemporaryDir dir; check(dir.isValid(),"UI fixture directory");
    p.receiver.reset(); p.streamerImage=playing::core::ImageReference{playing::core::ImageKind::LocalFile,image.toUtf8().toStdString(),""};
    const auto djPath=dir.filePath("dj.pcs"),rxPath=dir.filePath("rx.pcs"); ProtectedStore djStore(djPath),rxStore(rxPath); djStore.save(encodeProfile(p));
    Profile manualReceiver;manualReceiver.identity=createIdentity();rxStore.save(encodeProfile(manualReceiver));
    const auto pin=certificatePin(p.identity.certificate);
    QTcpServer reserve; check(reserve.listen(QHostAddress::LocalHost,0),"Reserve port"); const auto port=reserve.serverPort(); reserve.close();
    const auto history=dir.filePath("history.txt"); write(history,QByteArray::fromStdString(u8"22:01 : Sonny Fodera - Mind Still (Original Mix)\r\n"));
    Window sender(djPath,db.toUtf8().toStdString()),receiver(rxPath,db.toUtf8().toStdString());
    sender.setAttribute(Qt::WA_DontShowOnScreen); receiver.setAttribute(Qt::WA_DontShowOnScreen); sender.show(); receiver.show(); QApplication::processEvents();
    check(sender.width()<=460 && sender.height()<=780,"Dashboard still oversized");
    check(sender.screen()->availableGeometry().contains(sender.frameGeometry()),"Dashboard exceeds available desktop at current DPI");
    const auto normalSize=sender.size();
    sender.resize(420,420); QApplication::processEvents();
    auto dashboard=child<QScrollArea>(sender,"dashboardScroll");
    auto configure=child<QPushButton>(sender,"configure");
    check(sender.rect().contains(QRect(configure->mapTo(&sender,QPoint()),configure->size())),"Settings button clipped on short window");
    auto pairButton=child<QPushButton>(sender,"pairDashboard");
    dashboard->ensureWidgetVisible(pairButton); QApplication::processEvents();
    check(dashboard->viewport()->rect().contains(QRect(pairButton->mapTo(dashboard->viewport(),QPoint()),pairButton->size())),"Pair button unreachable by scrolling");
    check(dashboard->horizontalScrollBar()->maximum()==0,"Compact dashboard needs horizontal scrolling");
    check(sender.grab().save(output+".short.png"),"Short dashboard screenshot");
    sender.resize(normalSize); dashboard->verticalScrollBar()->setValue(0); QApplication::processEvents();
    auto settingsWindow=child<QDialog>(sender,"settingsDialog"); settingsWindow->setAttribute(Qt::WA_DontShowOnScreen); settingsWindow->show(); QApplication::processEvents();
    check(settingsWindow->screen()->availableGeometry().contains(settingsWindow->frameGeometry()),"Settings exceeds available desktop at current DPI");
    settingsWindow->resize(520,360); QApplication::processEvents();
    auto closeSettings=child<QPushButton>(sender,"closeSettings");
    check(settingsWindow->rect().contains(QRect(closeSettings->mapTo(settingsWindow,QPoint()),closeSettings->size())),"Settings close button clipped");
    auto settingsScroll=child<QScrollArea>(sender,"settingsScroll"); auto stopButton=child<QPushButton>(sender,"stop");
    settingsScroll->ensureWidgetVisible(stopButton); QApplication::processEvents();
    check(settingsScroll->viewport()->rect().contains(QRect(stopButton->mapTo(settingsScroll->viewport(),QPoint()),stopButton->size())),"Stop button unreachable in short settings");
    settingsWindow->close();
    check(child<QLabel>(sender,"network")->text().contains(playing::ui::string("●  Disconnected")),"Startup opened connection");
    check(child<QLabel>(sender,"track")->text()=="Esperando canción","Startup invented track");
    check(child<QLabel>(sender,"artwork")->pixmap().width()>0,"Profile fallback preview");
    child<QCheckBox>(receiver,"autoReconnect")->setChecked(true); check(decodeProfile(*rxStore.load()).autoReconnect,"GUI retry preference not protected/persisted");
    child<QCheckBox>(receiver,"autoReconnect")->setChecked(false); check(!decodeProfile(*rxStore.load()).autoReconnect,"GUI retry preference disable not saved");
    child<QSpinBox>(sender,"port")->setValue(port); child<QComboBox>(sender,"provider")->setCurrentIndex(4); child<QLineEdit>(sender,"history")->setText(history);
    child<QPushButton>(sender,"startSender")->click();
    check(child<QLabel>(sender,"status")->text().contains("Envío iniciado"),"Sender start button");
    check(child<QLabel>(sender,"track")->text()=="Esperando canción","Started detection without demand");
    QString newCode; bool named=false; QTimer pairing;
    pairing.setInterval(10); QObject::connect(&pairing,&QTimer::timeout,&sender,[&] {
        if(auto input=qobject_cast<QInputDialog *>(QApplication::activeModalWidget())) {if(!named) {named=true; input->setTextValue("SECOND-PC"); input->accept();}}
        else if(auto dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget())) {if(auto text=dialog->findChild<QPlainTextEdit *>()) {newCode=text->toPlainText(); pairing.stop(); dialog->accept();}}
    }); pairing.start(); child<QPushButton>(sender,"pairDevice")->click(); pairing.stop();
    check(named && !newCode.isEmpty(),"Pairing dialog flow");
    const auto savedPair=decodeProfile(*djStore.load()); const auto imported=parsePairingCode(newCode);
    check(savedPair.devices.size()==2 && savedPair.devices[1].name=="SECOND-PC" && savedPair.devices[1].credential.secret==imported.device.secret,"UI created/persisted credential");
    check(savedPair.sender.port==port && savedPair.sender.selection && savedPair.sender.selection->mode==playing::providers::Mode::History,"Sender preferences not persisted");
    const ReceiverSettings settings{"127.0.0.1",port,"OBS Playing Companion",pin,p.devices[0].credential};
    child<QLineEdit>(receiver,"pairingCode")->setText(pairingCode(settings)); child<QPushButton>(receiver,"connectReceiver")->click();
    check(wait([&] {return child<QLabel>(receiver,"track")->text()=="Mind Still (Original Mix)";}),"GUI native history through TLS");
    check(child<QLabel>(sender,"network")->text().contains(QString::fromUtf8(playing::ui::text("Conectado"))) && child<QLabel>(receiver,"network")->text().contains(QString::fromUtf8(playing::ui::text("Conectado"))),"GUI connection state");
    check(child<QLabel>(sender,"clientsTitle")->text()==playing::ui::string("CONNECTED CLIENTS")&&child<QLabel>(receiver,"clientsTitle")->text()=="PC DJ VINCULADO","Role-specific client heading");
    check(child<QLabel>(receiver,"linkedDj")->text().contains("Conectado")&&child<QListWidget>(receiver,"devices")->isHidden(),"Receiver falsely shows local clients");
    check(child<QListWidget>(sender,"devices")->item(0)->text().contains(QString::fromUtf8(playing::ui::text("Conectado"))) && child<QListWidget>(sender,"devices")->item(0)->text().contains("127.0.0.1"),"Actual authenticated client address");
    check(child<QLineEdit>(receiver,"pairingCode")->text().isEmpty(),"Pair code retained in edit");
    check(decodeProfile(*rxStore.load()).receiver->device.secret==p.devices[0].credential.secret,"Receiver pairing not persisted");
    receiver.close(); check(wait([&] {return child<QLabel>(sender,"network")->text().contains(playing::ui::string("●  Disconnected"));}),"Window close retained authenticated client");
    check(child<QLabel>(receiver,"track")->text()=="Esperando canción","Window close retained song"); receiver.show();
    child<QPushButton>(receiver,"reconnectReceiver")->click(); check(wait([&] {return child<QLabel>(receiver,"track")->text()=="Mind Still (Original Mix)";}),"Saved pairing reconnect after window close");
    check(sender.grab().save(output),"Dashboard screenshot");
    child<QListWidget>(sender,"devices")->setCurrentRow(0); child<QPushButton>(sender,"revokeDevice")->click();
    check(wait([&] {return child<QLabel>(receiver,"track")->text()=="Esperando canción";}),"Revocation kept remote song");
    check(decodeProfile(*djStore.load()).devices.size()==1 && decodeProfile(*djStore.load()).devices[0].name=="SECOND-PC","Revocation not persisted");
    check(child<QPushButton>(sender,"automaticPairSender")&&child<QPushButton>(receiver,"findDj"),"Guided pairing controls missing");
    child<QCheckBox>(receiver,"autoReceive")->setChecked(true);
    const auto oldIdentity=decodeProfile(*rxStore.load()).identity.privateKey.toDer();
    child<QPushButton>(receiver,"forgetReceiver")->click();
    const auto forgotten=decodeProfile(*rxStore.load());
    check(!forgotten.receiver&&forgotten.startup==StartupMode::Manual&&!forgotten.autoReconnect,"Forget retained pairing or automatic startup");
    check(!child<QCheckBox>(receiver,"autoReceive")->isEnabled()&&!child<QCheckBox>(receiver,"autoReconnect")->isChecked(),"Forget retained reconnection controls");
    check(forgotten.identity.privateKey.toDer()==oldIdentity,"Forget replaced local identity");
    check(child<QLabel>(receiver,"linkedDj")->text()=="Sin PC DJ vinculado","Forget retained linked DJ display");
    child<QPushButton>(sender,"stop")->click(); check(child<QLabel>(sender,"status")->text()=="Sin conexión","Stop button");
    sender.close(); receiver.close();
    check(decodeProfile(*djStore.load()).identity.privateKey.toDer()==p.identity.privateKey.toDer(),"Identity changed after close");
    {Window reopened(djPath,db.toUtf8().toStdString()); check(child<QListWidget>(reopened,"devices")->count()==1 && child<QListWidget>(reopened,"devices")->item(0)->text().startsWith("SECOND-PC"),"Revoked device restored"); check(child<QSpinBox>(reopened,"port")->value()==port && child<QComboBox>(reopened,"provider")->currentIndex()==4,"Sender settings restart");}
}
}
int main(int argc,char **argv) {playing::ui::setLanguage("es");
    std::cerr<<"Starting Qt UI runtime\n";
    QApplication app(argc,argv); app.setQuitOnLastWindowClosed(false);
    std::cerr<<"Qt UI runtime ready\n";
    QTimer modalGuard; modalGuard.setInterval(50); QObject::connect(&modalGuard,&QTimer::timeout,&app,[] {if(auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {++unexpectedDialogs; std::cerr<<"Unexpected application error dialog\n"; box->accept();}}); modalGuard.start();
    try {check(argc==4,"DB/output/profile-image arguments"); check(QSslSocket::setActiveBackend("schannel"),"Schannel backend"); std::cerr<<"Creating native identity\n"; auto p=profile(); std::cerr<<"Testing profile\n"; codec(p); std::cerr<<"Testing GUI\n"; gui(p,QString::fromLocal8Bit(argv[1]),QString::fromLocal8Bit(argv[2]),QString::fromLocal8Bit(argv[3])); automatic(QString::fromLocal8Bit(argv[1])); selectionPreview(QString::fromLocal8Bit(argv[1])); check(unexpectedDialogs==0,"Unexpected error dialog during UI flow"); std::cout<<"PASS "<<checks<<" Companion identity/profile/UI checks; loopback, same process\n"; return 0;}
    catch(const std::exception &e) {std::cerr<<"FAIL "<<e.what()<<'\n'; return 1;}
}
