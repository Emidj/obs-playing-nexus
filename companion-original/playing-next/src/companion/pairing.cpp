// SPDX-License-Identifier: MIT
#include "playing/companion/pairing.hpp"
#include "playing/companion/profile.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QSslConfiguration>
#include <QSslError>
#include <QSslSocket>
#include <QTcpServer>
#include <QThread>
#include <QStringList>
#include <QTimer>
#include <QUdpSocket>
#include <map>
#include <stdexcept>
namespace playing::companion {
namespace {
void owner(const QObject *host) {if(QThread::currentThread()!=host->thread()) throw std::logic_error("Pairing owner thread required");}
bool local(const QHostAddress &address) {
    bool ok=false; const auto ip=address.toIPv4Address(&ok);
    return ok && ((ip>>24)==10 || (ip>>24)==127 || (ip&0xfff00000)==0xac100000 || (ip>>16)==0xc0a8 || (ip>>16)==0xa9fe);
}
bool name(const QString &text) {return !text.trimmed().isEmpty() && text.size()<=80 && !text.contains(QChar::Null);}
bool hex(const QByteArray &text,int size) {if(text.size()!=size) return false; for(auto c:text) if(!((c>='0'&&c<='9')||(c>='a'&&c<='f'))) return false; return true;}
QByteArray frame(const QByteArray &data) {QByteArray result(4,'\0'); const auto n=static_cast<quint32>(data.size()); for(unsigned i=0;i<4;++i) result[static_cast<qsizetype>(i)]=static_cast<char>((n>>((3-i)*8))&255); return result+data;}
int packet(QSslSocket *socket,QByteArray &buffer,QByteArray &data) {
    buffer+=socket->read(4101-buffer.size()); if(buffer.size()>4100) return -1;
    if(buffer.size()<4) return 0; quint32 n=0; for(int i=0;i<4;++i) n=(n<<8)|static_cast<unsigned char>(buffer[i]);
    if(n<4 || n>4096) return -1; if(buffer.size()<n+4) return 0;
    if(buffer.size()!=n+4 || socket->bytesAvailable()>0) return -1;
    data=buffer.mid(4); buffer.fill('\0'); buffer.clear(); return 1;
}
class Listener final : public QTcpServer {
public: using QTcpServer::QTcpServer; std::function<void(qintptr)> incoming;
protected: void incomingConnection(qintptr descriptor) override {incoming(descriptor);}
};
QJsonObject parse(const QByteArray &data) {QJsonParseError error; const auto document=QJsonDocument::fromJson(data,&error); return error.error==QJsonParseError::NoError&&document.isObject()?document.object():QJsonObject{};}
bool valid(const DiscoveredDj &dj) {return name(dj.name)&&local(QHostAddress(dj.address))&&dj.pairingPort>0&&dj.dataPort>=1024&&dj.pin.size()==32;}
}
QString pairingFingerprint(const QByteArray &pin) {
    if(pin.size()!=32) return {}; const auto text=QString::fromLatin1(pin.toHex().first(32)).toUpper(); QStringList groups;
    for(int i=0;i<32;i+=4) groups.append(text.mid(i,4)); return groups.join(' ');
}
struct PairingServer::Impl {
    struct Peer {QByteArray buffer,id; bool pending=false,issued=false;};
    PairingServer *host; Listener listener; QUdpSocket udp; QTimer expiry;
    ServerIdentity identity; QString deviceName; quint16 dataPort=0; unsigned queries=0,requests=0;
    std::map<QSslSocket *,Peer> peers;
    explicit Impl(PairingServer *h):host(h),listener(h),udp(h),expiry(h) {
        expiry.setSingleShot(true); QObject::connect(&expiry,&QTimer::timeout,host,[this] {host->stop();});
        listener.incoming=[this](qintptr descriptor) {accept(descriptor);};
        QObject::connect(&udp,&QUdpSocket::readyRead,host,[this] {
            for(unsigned count=0;count<32&&udp.hasPendingDatagrams();++count) {
                char bytes[1025]; QHostAddress address; quint16 port=0; const auto n=udp.readDatagram(bytes,sizeof(bytes),&address,&port);
                if(n!=36 || !local(address) || ++queries>128) continue;
                const QByteArray request(bytes,static_cast<qsizetype>(n)); if(!request.startsWith("NPD1")||!hex(request.mid(4),32)) continue;
                const QJsonObject object{{"nonce",QString::fromLatin1(request.mid(4))},{"name",deviceName},{"pin",QString::fromLatin1(certificatePin(identity.certificate).toHex())},{"pairPort",listener.serverPort()},{"dataPort",dataPort}};
                udp.writeDatagram("NPD2"+QJsonDocument(object).toJson(QJsonDocument::Compact),address,port);
            }
        });
    }
    void accept(qintptr descriptor) {
        auto socket=new QSslSocket(host); if(!socket->setSocketDescriptor(descriptor)) {delete socket; return;}
        if(peers.size()>=4 || requests>=16 || !local(socket->peerAddress())) {socket->abort(); socket->deleteLater(); return;}
        auto config=QSslConfiguration::defaultConfiguration(); config.setProtocol(QSsl::TlsV1_2OrLater); config.setPeerVerifyMode(QSslSocket::VerifyNone); config.setLocalCertificate(identity.certificate); config.setPrivateKey(identity.privateKey); socket->setSslConfiguration(config); socket->setReadBufferSize(4101);
        peers.emplace(socket,Peer{}); auto timeout=new QTimer(socket); timeout->setSingleShot(true); timeout->start(45000); QObject::connect(timeout,&QTimer::timeout,socket,[socket] {socket->abort();});
        QObject::connect(socket,&QSslSocket::disconnected,host,[this,socket] {peers.erase(socket); socket->deleteLater();});
        QObject::connect(socket,&QSslSocket::errorOccurred,host,[socket](QAbstractSocket::SocketError) {socket->abort();});
        QObject::connect(socket,&QSslSocket::readyRead,host,[this,socket] {
            const auto found=peers.find(socket); if(found==peers.end()) return; auto &peer=found->second;
            if(!socket->isEncrypted() || peer.pending || peer.issued) {socket->abort(); return;}
            QByteArray payload; const auto result=packet(socket,peer.buffer,payload); if(result<0) {socket->abort(); return;} if(!result) return;
            const auto object=parse(payload.mid(4)); const auto device=object.value("name").toString();
            if(!payload.startsWith("NPQ1")||object.size()!=1||!name(device)||++requests>16) {socket->abort(); return;}
            for(const auto &entry:peers) if(entry.second.pending) {socket->abort(); return;}
            peer.pending=true; peer.id=newDeviceCredential().id; const auto id=peer.id;
            if(host->requested) host->requested(id,device); else host->reject(id);
        });
        socket->startServerEncryption();
    }
};
PairingServer::PairingServer(QObject *parent):QObject(parent),impl_(std::make_unique<Impl>(this)) {}
PairingServer::~PairingServer() {stop();}
bool PairingServer::open(const ServerIdentity &identity,quint16 dataPort,const QString &deviceName,const QHostAddress &bind,quint16 discovery,int lifetimeMs) {
    owner(this); stop(); if(identity.certificate.isNull()||identity.privateKey.isNull()||dataPort<1024||!name(deviceName)||lifetimeMs<100||lifetimeMs>120000||bind.protocol()!=QAbstractSocket::IPv4Protocol) return false;
    impl_->identity=identity; impl_->dataPort=dataPort; impl_->deviceName=deviceName; impl_->queries=impl_->requests=0;
    if(!impl_->listener.listen(bind,0)||!impl_->udp.bind(bind,discovery,QUdpSocket::DontShareAddress)) {stop(); return false;}
    impl_->expiry.start(lifetimeMs); if(changed) changed(); return true;
}
void PairingServer::stop() {owner(this); impl_->expiry.stop(); impl_->listener.close(); impl_->udp.close(); for(const auto &entry:impl_->peers) {entry.first->disconnect(this); entry.first->abort(); entry.first->deleteLater();} impl_->peers.clear(); if(changed) changed();}
bool PairingServer::active() const {owner(this); return impl_->listener.isListening();}
quint16 PairingServer::discoverySocketPort() const {owner(this); return impl_->udp.localPort();}
bool PairingServer::pending(const QByteArray &id) const {owner(this); for(const auto &entry:impl_->peers) if(entry.second.pending&&entry.second.id==id) return true; return false;}
bool PairingServer::approve(const QByteArray &id,const DeviceCredential &credential) {
    owner(this); for(auto &entry:impl_->peers) if(entry.second.pending&&entry.second.id==id) {
        const ReceiverSettings settings{entry.first->localAddress().toString(),impl_->dataPort,"OBS Playing Companion",certificatePin(impl_->identity.certificate),credential};
        QByteArray data; try {data=frame("NPR1"+pairingCode(settings).toLatin1());} catch(...) {return false;}
        entry.second.pending=false; entry.second.issued=true;
        return entry.first->isEncrypted()&&entry.first->write(data)==data.size();
    } return false;
}
void PairingServer::reject(const QByteArray &id) {owner(this); for(auto &entry:impl_->peers) if(entry.second.id==id) {entry.first->abort(); return;}}
struct PairingClient::Impl {
    PairingClient *host; QUdpSocket udp; QTimer expiry,deadline; QSslSocket *socket=nullptr;
    QByteArray nonce,buffer; std::vector<DiscoveredDj> devices; QString error; DiscoveredDj selected; unsigned generation=0;
    explicit Impl(PairingClient *h):host(h),udp(h),expiry(h),deadline(h) {
        expiry.setSingleShot(true); deadline.setSingleShot(true);
        QObject::connect(&expiry,&QTimer::timeout,host,[this] {udp.close(); notify();});
        QObject::connect(&deadline,&QTimer::timeout,host,[this] {fail("La solicitud de vinculación ha caducado.");});
        QObject::connect(&udp,&QUdpSocket::readyRead,host,[this] {
            for(unsigned count=0;count<32&&udp.hasPendingDatagrams();++count) {
                char bytes[1025]; QHostAddress source; const auto n=udp.readDatagram(bytes,sizeof(bytes),&source);
                if(n<5||n>1024||!local(source)) continue; const QByteArray payload(bytes,static_cast<qsizetype>(n)); if(!payload.startsWith("NPD2")) continue;
                const auto object=parse(payload.mid(4)); if(object.size()!=5||object.value("nonce").toString().toLatin1()!=nonce) continue;
                const auto pair=object.value("pairPort"),data=object.value("dataPort"); const auto pin=object.value("pin").toString().toLatin1();
                if(!pair.isDouble()||pair.toDouble()!=pair.toInt()||pair.toInt()<1||pair.toInt()>65535||!data.isDouble()||data.toDouble()!=data.toInt()||data.toInt()<1024||data.toInt()>65535||!hex(pin,64)) continue;
                DiscoveredDj candidate{object.value("name").toString(),source.toString(),static_cast<quint16>(pair.toInt()),static_cast<quint16>(data.toInt()),QByteArray::fromHex(pin)};
                if(!valid(candidate)||devices.size()>=16) continue;
                bool duplicate=false; for(const auto &known:devices) if(known.address==candidate.address&&known.pairingPort==candidate.pairingPort&&known.pin==candidate.pin) duplicate=true;
                if(!duplicate) {devices.push_back(candidate); notify();}
            }
        });
    }
    void notify() {if(host->changed) host->changed();}
    void closeSocket() {deadline.stop(); if(socket) {socket->disconnect(host); socket->abort(); socket->deleteLater(); socket=nullptr;} buffer.fill('\0'); buffer.clear();}
    void fail(const QString &reason) {closeSocket(); error=reason; notify();}
};
PairingClient::PairingClient(QObject *parent):QObject(parent),impl_(std::make_unique<Impl>(this)) {}
PairingClient::~PairingClient() {stop();}
bool PairingClient::search(const QHostAddress &target,quint16 port) {
    owner(this); stop(); if(!port||(!local(target)&&target!=QHostAddress::Broadcast)) return false;
    impl_->devices.clear(); impl_->error.clear(); impl_->nonce=newDeviceCredential().id;
    if(!impl_->udp.bind(QHostAddress(QHostAddress::AnyIPv4),0)) return false;
    const auto generation=impl_->generation; const auto send=[this,target,port,generation] {if(generation==impl_->generation&&impl_->expiry.isActive()) impl_->udp.writeDatagram("NPD1"+impl_->nonce,target,port);};
    impl_->expiry.start(8000); send(); QTimer::singleShot(500,this,send); QTimer::singleShot(1500,this,send); impl_->notify(); return true;
}
std::vector<DiscoveredDj> PairingClient::devices() const {owner(this); return impl_->devices;}
bool PairingClient::request(const DiscoveredDj &candidate,const QString &deviceName) {
    owner(this); impl_->closeSocket(); impl_->error.clear(); if(!valid(candidate)||!name(deviceName)) return false;
    impl_->udp.close(); impl_->expiry.stop(); impl_->selected=candidate; auto socket=new QSslSocket(this); impl_->socket=socket; socket->setReadBufferSize(4101);
    auto config=QSslConfiguration::defaultConfiguration(); config.setProtocol(QSsl::TlsV1_2OrLater); config.setPeerVerifyMode(QSslSocket::VerifyPeer); socket->setSslConfiguration(config); socket->setPeerVerifyName("OBS Playing Companion");
    QObject::connect(socket,&QSslSocket::sslErrors,this,[this,socket](const QList<QSslError> &errors) {
        if(impl_->socket!=socket) return;
        if(certificatePin(socket->peerCertificate())!=impl_->selected.pin) {impl_->fail("La identidad del PC DJ ha cambiado."); return;}
        for(const auto &error:errors) if(error.error()!=QSslError::SelfSignedCertificate&&error.error()!=QSslError::CertificateUntrusted) {impl_->fail("El certificado del PC DJ no es válido."); return;}
        socket->ignoreSslErrors(errors);
    });
    QObject::connect(socket,&QSslSocket::encrypted,this,[this,socket,deviceName] {
        if(impl_->socket!=socket) return; if(certificatePin(socket->peerCertificate())!=impl_->selected.pin) {impl_->fail("La identidad del PC DJ ha cambiado."); return;}
        const auto data=frame("NPQ1"+QJsonDocument(QJsonObject{{"name",deviceName}}).toJson(QJsonDocument::Compact)); if(socket->write(data)!=data.size()) impl_->fail("No se pudo solicitar la vinculación.");
    });
    QObject::connect(socket,&QSslSocket::readyRead,this,[this,socket] {
        if(impl_->socket!=socket) return; QByteArray payload; const auto result=packet(socket,impl_->buffer,payload);
        if(result<0) {impl_->fail("Respuesta de vinculación incompatible."); return;} if(!result) return;
        ReceiverSettings settings; try {if(!payload.startsWith("NPR1")) throw std::invalid_argument("Pair response"); settings=parsePairingCode(QString::fromLatin1(payload.mid(4)));} catch(...) {impl_->fail("Respuesta de vinculación incompatible."); return;}
        if(settings.address!=impl_->selected.address||settings.port!=impl_->selected.dataPort||settings.certificateSha256!=impl_->selected.pin) {impl_->fail("La identidad o dirección del PC DJ ha cambiado."); return;}
        payload.fill('\0'); impl_->closeSocket(); if(completed) completed(settings);
    });
    QObject::connect(socket,&QSslSocket::disconnected,this,[this,socket] {if(impl_->socket==socket) impl_->fail("La vinculación se canceló o fue rechazada.");});
    QObject::connect(socket,&QSslSocket::errorOccurred,this,[this,socket](QAbstractSocket::SocketError) {if(impl_->socket==socket) impl_->fail("No se pudo conectar para vincular.");});
    impl_->deadline.start(45000); socket->connectToHostEncrypted(candidate.address,candidate.pairingPort); return true;
}
void PairingClient::stop() {owner(this); ++impl_->generation; impl_->expiry.stop(); impl_->udp.close(); impl_->closeSocket(); impl_->error.clear();}
QString PairingClient::error() const {owner(this); return impl_->error;}
bool PairingClient::searching() const {owner(this); return impl_->expiry.isActive();}
} // namespace playing::companion
