// SPDX-License-Identifier: MIT
#include "playing/companion/transport.hpp"
#include <windows.h>
#include <bcrypt.h>
#include <QCryptographicHash>
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QSaveFile>
#include <QStandardPaths>
#include <QThreadPool>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QSslConfiguration>
#include <QSslError>
#include <QSslSocket>
#include <QTcpServer>
#include <QThread>
#include <QTimer>
#include <map>
#include <stdexcept>

namespace playing::companion {
namespace {
namespace net = providers::network;
constexpr qsizetype maxPacket = static_cast<qsizetype>(net::maxFrameBytes);
constexpr qsizetype maxCoverBytes=60*1024;
const core::ImageReference *localCover(const core::Snapshot &snapshot) {
    const auto track=snapshot.running?snapshot.track:nullptr;
    return track&&track->artwork.status==core::ArtworkStatus::Ready&&track->artwork.image
        &&track->artwork.image->kind==core::ImageKind::LocalFile?&*track->artwork.image:nullptr;
}
std::string coverKey(const core::ImageReference &ref) {return std::to_string(ref.location.size())+":"+ref.location+ref.version;}
// Only the selected cover is read, on a worker. Paths never cross the wire.
QByteArray prepareCover(const core::ImageReference &ref) {
    const auto path=QString::fromUtf8(ref.location);
    if(path.size()<3||path[1]!=':'||(path[2]!='/'&&path[2]!='\\'))return {};
    const auto drive=GetDriveTypeW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(path.first(3)).utf16()));
    if(drive!=DRIVE_FIXED&&drive!=DRIVE_REMOVABLE)return {};
    QFile file(path);if(!file.open(QIODevice::ReadOnly)||file.size()<8||file.size()>4*1024*1024)return {};
    auto bytes=file.read(4*1024*1024+1);if(bytes.size()>4*1024*1024||file.error()!=QFileDevice::NoError)return {};
    QBuffer input(&bytes);input.open(QIODevice::ReadOnly);QImageReader reader(&input);reader.setAutoTransform(true);
    const auto format=reader.format().toLower();if(format!="png"&&format!="jpeg"&&format!="jpg"&&format!="webp")return {};
    const auto size=reader.size();if(!size.isValid()||size.width()>4096||size.height()>4096||qint64(size.width())*size.height()>4*1024*1024)return {};
    if(size.width()>512||size.height()>512)reader.setScaledSize(size.scaled(512,512,Qt::KeepAspectRatio));
    auto image=reader.read().convertToFormat(QImage::Format_ARGB32);if(image.isNull())return {};
    bool transparent=false;for(int y=0;y<image.height()&&!transparent;++y){const auto row=reinterpret_cast<const QRgb *>(image.constScanLine(y));for(int x=0;x<image.width();++x)if(qAlpha(row[x])!=255){transparent=true;break;}}
    for(const int edge:{512,256,128,96}){
        const auto scaled=image.width()>edge||image.height()>edge?image.scaled(edge,edge,Qt::KeepAspectRatio,Qt::SmoothTransformation):image;
        QByteArray encoded;{QBuffer output(&encoded);output.open(QIODevice::WriteOnly);if(!scaled.save(&output,"PNG"))return {};}
        if(encoded.size()<=maxCoverBytes)return encoded;
        if(!transparent)for(const int quality:{85,70,55}){encoded.clear();QBuffer output(&encoded);output.open(QIODevice::WriteOnly);if(scaled.save(&output,"JPEG",quality)&&encoded.size()<=maxCoverBytes)return encoded;}
    }
    return {};
}
// Decode/validate before accepting bytes into a private, bounded content cache.
std::optional<core::ImageReference> saveCover(QByteArray bytes,const QByteArray &digest,const QString &root) {
    if(bytes.size()<8||bytes.size()>maxCoverBytes||QCryptographicHash::hash(bytes,QCryptographicHash::Sha256)!=digest)return {};
    QBuffer input(&bytes);input.open(QIODevice::ReadOnly);QImageReader reader(&input);
    const auto format=reader.format().toLower();if(format!="png"&&format!="jpeg"&&format!="jpg")return {};
    const auto size=reader.size();if(!size.isValid()||size.width()>512||size.height()>512||reader.read().isNull())return {};
    if(!QDir().mkpath(root)||QFileInfo(root).isSymLink())return {};
    const auto id=QString::fromLatin1(digest.toHex());const auto path=QDir(root).filePath(id+(format=="png"?".png":".jpg"));
    if(QFileInfo(path).isSymLink())return {};
    QFile existing(path);
    if(existing.open(QIODevice::ReadOnly)&&existing.size()<=maxCoverBytes&&QCryptographicHash::hash(existing.readAll(),QCryptographicHash::Sha256)==digest)
        return core::ImageReference{core::ImageKind::LocalFile,path.toUtf8().toStdString(),id.toStdString()};
    existing.close();
    qint64 resident=0;for(const auto &entry:QDir(root).entryInfoList(QDir::Files))resident+=entry.size();
    if(resident+bytes.size()>64*1024*1024)return {};
    QSaveFile file(path);file.setDirectWriteFallback(false);
    if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit())return {};
    return core::ImageReference{core::ImageKind::LocalFile,path.toUtf8().toStdString(),id.toStdString()};
}
QByteArray randomBytes() {
    QByteArray bytes(32, '\0');
    if (BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(bytes.data()), 32, BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
        throw std::runtime_error("System random generator unavailable");
    return bytes;
}
void owner(const QObject *object) {
    if (QThread::currentThread() != object->thread()) throw std::logic_error("Companion transport requires owner event loop");
}
bool validCredential(const DeviceCredential &credential) {
    if (credential.id.size() != 32 || credential.secret.size() != 32) return false;
    for (const auto c : credential.id) if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    return true;
}
bool equalSecret(const QByteArray &a, const QByteArray &b) {
    if (a.size() != 32 || b.size() != 32) return false;
    unsigned result = 0;
    for (qsizetype i = 0; i < 32; ++i) result |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
    return result == 0;
}
QByteArray packet(QByteArray payload) {
    const auto n = static_cast<quint32>(payload.size());
    QByteArray framed(4, '\0');
    for (unsigned i = 0; i < 4; ++i) framed[static_cast<qsizetype>(i)] = static_cast<char>((n >> ((3-i)*8)) & 255);
    framed += payload; return framed;
}
// 0=incomplete, -1=invalid, 1=one complete frame. Bounded before allocation.
int extract(QByteArray &buffer, QByteArray &frame) {
    if (buffer.size() < 4) return 0;
    quint32 size = 0; for (qsizetype i = 0; i < 4; ++i) size = (size << 8) | static_cast<unsigned char>(buffer[i]);
    if (size < 4 || size > static_cast<quint32>(maxPacket-4)) return -1;
    if (buffer.size() < static_cast<qsizetype>(size)+4) return 0;
    frame = buffer.first(static_cast<qsizetype>(size)+4); buffer.remove(0, frame.size()); return 1;
}
bool send(QSslSocket *socket, const QByteArray &frame) {
    if (!socket->isEncrypted() || frame.size() > maxPacket || socket->bytesToWrite()+frame.size() > maxPacket*2) return false;
    return socket->write(frame) == frame.size();
}
class Listener final : public QTcpServer {
public:
    explicit Listener(QObject *parent) : QTcpServer(parent) {}
    std::function<void(qintptr)> accept;
protected:
    void incomingConnection(qintptr descriptor) override { accept(descriptor); }
};
}
DeviceCredential newDeviceCredential() { return {randomBytes().toHex().first(32), randomBytes()}; }
QByteArray certificatePin(const QSslCertificate &certificate) {
    return certificate.isNull() ? QByteArray{} : QCryptographicHash::hash(certificate.toDer(), QCryptographicHash::Sha256);
}

struct Sender::Impl {
    struct Peer {
        QByteArray buffer, device; std::string epoch;
        std::uint64_t sequence=0,snapshotSequence=0; bool authenticated=false,artwork=false; QTimer *deadline=nullptr;
    };
    Sender *host; Listener listener; QTimer heartbeat;
    ServerIdentity identity; core::SnapshotPtr latest=std::make_shared<const core::Snapshot>();
    std::map<QByteArray,QByteArray> devices; std::map<QSslSocket *,Peer> peers;
    QThreadPool images;std::string pendingCover,preparedKey;QByteArray preparedCover;std::uint64_t imageGeneration=0;
    explicit Impl(Sender *h) : host(h), listener(h), heartbeat(h) {
        listener.accept=[this](qintptr descriptor) { accept(descriptor); };
        images.setMaxThreadCount(1);images.setExpiryTimeout(0);
        heartbeat.setInterval(5000);
        QObject::connect(&heartbeat,&QTimer::timeout,host,[this] {
            QList<QSslSocket *> active; for(const auto &p:peers) if(p.second.authenticated) active.append(p.first);
            for (auto socket:active) {const auto found=peers.find(socket); if(found==peers.end()) continue; auto &peer=found->second;
                net::Message m{peer.epoch,++peer.sequence,net::MessageKind::Heartbeat,{}};
                const auto encoded=net::encode(m); if(!encoded || !send(socket,QByteArray::fromStdString(*encoded))) socket->abort();
            }
        });
    }
    ~Impl(){images.clear();images.waitForDone();}
    void image(QSslSocket *socket,Peer &peer) {
        const auto ref=localCover(*latest);
        if(!peer.artwork||!peer.snapshotSequence||!ref||preparedKey!=coverKey(*ref)||preparedCover.isEmpty())return;
        QByteArray body="NPI1"+QByteArray::fromStdString(peer.epoch);for(int i=7;i>=0;--i)body+=char((peer.snapshotSequence>>(i*8))&255);
        body+=QCryptographicHash::hash(preparedCover,QCryptographicHash::Sha256);body+=preparedCover;
        if(!send(socket,packet(body)))socket->abort();
    }
    void prepare() {
        const auto ref=localCover(*latest);
        bool wanted=false;for(const auto &p:peers)if(p.second.authenticated&&p.second.artwork)wanted=true;
        if(!wanted||!ref){++imageGeneration;pendingCover.clear();images.clear();return;}
        const auto key=coverKey(*ref);if(key==preparedKey||key==pendingCover)return;
        pendingCover=key;const auto generation=++imageGeneration;const auto reference=*ref;images.clear();
        images.start([this,key,generation,reference]{
            const auto encoded=prepareCover(reference);
            QMetaObject::invokeMethod(host,[this,key,generation,encoded]{
                if(generation!=imageGeneration||pendingCover!=key)return;
                pendingCover.clear();preparedKey=key;preparedCover=encoded;
                QList<QSslSocket *> active;for(const auto &p:peers)if(p.second.authenticated&&p.second.artwork)active.append(p.first);
                for(auto socket:active){const auto it=peers.find(socket);if(it!=peers.end())image(socket,it->second);}
            },Qt::QueuedConnection);
        });
    }
    std::size_t count() const { std::size_t n=0; for(const auto &p:peers) if(p.second.authenticated) ++n; return n; }
    void notify() { if(count()==0) heartbeat.stop(); else if(!heartbeat.isActive()) heartbeat.start(); if(host->clientsChanged) host->clientsChanged(); }
    void snapshot(QSslSocket *socket,Peer &peer) {
        peer.snapshotSequence=++peer.sequence;
        const auto encoded=net::encode(net::fromSnapshot(*latest,peer.epoch,peer.snapshotSequence));
        if(!encoded || !send(socket,QByteArray::fromStdString(*encoded))) socket->abort();
        else image(socket,peer);
    }
    void accept(qintptr descriptor) {
        auto socket=new QSslSocket(host);
        if(!socket->setSocketDescriptor(descriptor)) { delete socket; return; }
        if(peers.size()>=8) { socket->abort(); delete socket; return; }
        socket->setReadBufferSize(maxPacket*2);
        auto config=QSslConfiguration::defaultConfiguration(); config.setProtocol(QSsl::TlsV1_2OrLater);
        config.setPeerVerifyMode(QSslSocket::VerifyNone); config.setLocalCertificate(identity.certificate); config.setPrivateKey(identity.privateKey);
        socket->setSslConfiguration(config);
        Peer peer; peer.deadline=new QTimer(socket); peer.deadline->setSingleShot(true); peer.deadline->setInterval(10000);
        QObject::connect(peer.deadline,&QTimer::timeout,socket,[socket] {socket->abort();}); peer.deadline->start();
        peers.emplace(socket,std::move(peer));
        QObject::connect(socket,&QSslSocket::readyRead,host,[this,socket] {read(socket);});
        QObject::connect(socket,&QSslSocket::disconnected,host,[this,socket] {peers.erase(socket); socket->deleteLater(); notify();});
        QObject::connect(socket,&QSslSocket::errorOccurred,host,[socket](QAbstractSocket::SocketError) {socket->abort();});
        socket->startServerEncryption();
    }
    void read(QSslSocket *socket) {
        const auto found=peers.find(socket); if(found==peers.end()) return;
        auto &peer=found->second;
        if(!socket->isEncrypted()) {socket->abort(); return;}
        if(peer.authenticated){
            peer.buffer+=socket->read(9-peer.buffer.size());QByteArray frame;const auto status=extract(peer.buffer,frame);
            if(peer.buffer.size()>8||status<0){socket->abort();return;}if(!status)return;
            if(frame!=packet("NPC1")||peer.artwork||!peer.buffer.isEmpty()||socket->bytesAvailable()>0){socket->abort();return;}
            peer.artwork=true;prepare();snapshot(socket,peer);return;
        }
        // Authentication packet is exactly 72 framed bytes; cap before read.
        peer.buffer+=socket->read(73-peer.buffer.size());
        if(peer.buffer.size()>72) {socket->abort(); return;}
        QByteArray frame; const auto status=extract(peer.buffer,frame);
        if(status<0) {socket->abort(); return;} if(status==0) return;
        if(frame.size()!=72 || frame.mid(4,4)!="NPA1" || !peer.buffer.isEmpty() || socket->bytesAvailable()>0) {socket->abort(); return;}
        const auto id=frame.mid(8,32), secret=frame.mid(40,32); const auto device=devices.find(id);
        if(device==devices.end() || !equalSecret(device->second,secret)) {socket->abort(); return;}
        for(const auto &p:peers) if(p.first!=socket && p.second.authenticated && p.second.device==id) {socket->abort(); return;}
        peer.buffer.fill('\0'); peer.buffer.clear(); frame.fill('\0');
        // Opaque 32-byte epoch remains valid for older clients. ART1 advertises
        // optional image support; 21 random bytes preserve 168 bits of entropy.
        peer.device=id; peer.epoch=("ART1"+randomBytes().first(21).toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals)).toStdString(); peer.authenticated=true; peer.deadline->stop();
        if(!send(socket,packet("NPW1"+QByteArray::fromStdString(peer.epoch)))) {socket->abort(); return;}
        snapshot(socket,peer); notify();
    }
    void stop() {
        ++imageGeneration;pendingCover.clear();images.clear();
        listener.close(); heartbeat.stop();
        for(auto &p:peers) {QObject::disconnect(p.first,nullptr,host,nullptr); p.first->abort(); delete p.first;}
        peers.clear(); identity={}; notify();
    }
};
Sender::Sender(QObject *parent) : QObject(parent),impl_(std::make_unique<Impl>(this)) {}
Sender::~Sender() { clientsChanged={}; impl_->stop(); }
bool Sender::listen(const ServerIdentity &identity,const QString &address,quint16 port) {
    owner(this); if(impl_->listener.isListening() || identity.certificate.isNull() || identity.privateKey.isNull()
        || !QSslSocket::supportsSsl()) return false;
    const QHostAddress bind(address); if(bind.isNull()) return false;
    impl_->identity=identity; return impl_->listener.listen(bind,port);
}
quint16 Sender::port() const {owner(this); return impl_->listener.serverPort();}
bool Sender::pair(const DeviceCredential &credential) {
    owner(this); if(!validCredential(credential) || impl_->devices.size()>=16 || impl_->devices.count(credential.id)) return false;
    impl_->devices.emplace(credential.id,credential.secret); return true;
}
bool Sender::revoke(const QByteArray &id) {
    owner(this); const auto found=impl_->devices.find(id); if(found==impl_->devices.end()) return false;
    found->second.fill('\0'); impl_->devices.erase(found);
    // abort may synchronously erase peers; do not iterate while invoking it.
    QList<QSslSocket *> close; for(const auto &p:impl_->peers) if(p.second.device==id) close.append(p.first);
    for(auto socket:close) socket->abort(); return true;
}
void Sender::publish(core::SnapshotPtr snapshot) {
    owner(this); if(!snapshot || snapshot==impl_->latest) return;
    // Validate complete export before replacing the latest state.
    if(!net::encode(net::fromSnapshot(*snapshot,"validation",1))) throw std::invalid_argument("Invalid Companion snapshot");
    impl_->latest=std::move(snapshot);
    impl_->prepare();
    QList<QSslSocket *> active; for(const auto &p:impl_->peers) if(p.second.authenticated) active.append(p.first);
    for(auto socket:active) {const auto p=impl_->peers.find(socket); if(p!=impl_->peers.end()) impl_->snapshot(socket,p->second);}
}
std::size_t Sender::clientCount() const {owner(this); return impl_->count();}
std::vector<ConnectedClient> Sender::clients() const {
    owner(this); std::vector<ConnectedClient> result;
    for(const auto &p:impl_->peers) if(p.second.authenticated) result.push_back({p.second.device,p.first->peerAddress().toString()});
    return result;
}
void Sender::stop() {owner(this); impl_->stop();}

struct Receiver::Impl {
    Receiver *host; std::shared_ptr<net::Channel> channel; QSslSocket *socket=nullptr;
    QElapsedTimer clock; QTimer deadline,retry; QByteArray buffer; ReceiverSettings settings;
    ReceiverSettings retrySettings;
    bool autoReconnect=false; unsigned retryAttempt=0;
    ConnectionFailure failure=ConnectionFailure::None;
    bool authenticated=false,imagesSupported=false; QString error;
    QThreadPool images;QString cacheRoot;std::uint64_t imageGeneration=0;std::string stream;
    Impl(Receiver *h,std::shared_ptr<net::Channel> c,QString root) : host(h),channel(std::move(c)),deadline(h),retry(h),cacheRoot(std::move(root)) {
        if(!channel) throw std::invalid_argument("Receiver requires Network Channel"); clock.start();
        if(cacheRoot.isEmpty())cacheRoot=QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)).filePath("OBS Playing Next/Remote Artwork Cache");
        images.setMaxThreadCount(1);images.setExpiryTimeout(0);
        deadline.setSingleShot(true); QObject::connect(&deadline,&QTimer::timeout,host,[this] {fail("Connection timeout",ConnectionFailure::Timeout);});
        retry.setSingleShot(true); QObject::connect(&retry,&QTimer::timeout,host,[this] {
            const auto attempt=retryAttempt; const auto saved=retrySettings;
            host->connectTo(saved); retryAttempt=attempt;
        });
    }
    ~Impl(){images.clear();images.waitForDone();}
    double now() const {return static_cast<double>(clock.elapsed())/1000.0;}
    void notify() {if(host->changed) host->changed();}
    void stop() {
        ++imageGeneration;images.clear();stream.clear();imagesSupported=false;
        deadline.stop(); authenticated=false; buffer.clear();
        if(socket) {auto closing=socket; socket=nullptr; QObject::disconnect(closing,nullptr,host,nullptr); closing->abort(); closing->deleteLater();}
        channel->disconnect(); settings.device.secret.fill('\0'); settings={};
    }
    void fail(const QString &reason,ConnectionFailure kind=ConnectionFailure::Protocol) {
        error=reason; failure=kind; stop();
        if(autoReconnect && (kind==ConnectionFailure::Transport || kind==ConnectionFailure::Timeout)) {
            const unsigned seconds=retryAttempt>=5?30u:(1u<<retryAttempt); if(retryAttempt<5) ++retryAttempt;
            retry.start(static_cast<int>(seconds*1000));
        }
        notify();
    }
    bool pin() const {return socket && certificatePin(socket->peerCertificate())==settings.certificateSha256;}
    bool image(const QByteArray &frame){
        if(!imagesSupported||frame.size()<88||frame.size()>maxCoverBytes+80)return false;
        const auto epoch=frame.mid(8,32).toStdString();std::uint64_t sequence=0;
        for(int i=40;i<48;++i)sequence=(sequence<<8)|static_cast<unsigned char>(frame[i]);
        if(!channel->acceptsArtwork(epoch,sequence))return true; // Stale image cannot alter the next song.
        const auto digest=frame.mid(48,32),bytes=frame.mid(80);const auto generation=imageGeneration;const auto root=cacheRoot;
        images.clear();images.start([this,epoch,sequence,digest,bytes,generation,root]{
            const auto saved=saveCover(bytes,digest,root);
            QMetaObject::invokeMethod(host,[this,epoch,sequence,generation,saved]{
                if(generation!=imageGeneration||!saved)return;
                if(channel->updateArtwork(epoch,sequence,{core::ArtworkStatus::Ready,*saved}))notify();
            },Qt::QueuedConnection);
        });return true;
    }
    void read() {
        if(!socket || !socket->isEncrypted()) {fail("Unencrypted connection"); return;}
        buffer+=socket->read(maxPacket-buffer.size());
        for(unsigned n=0;n<8;++n) {
            QByteArray frame; const auto status=extract(buffer,frame);
            if(status<0) {fail("Invalid network frame"); return;} if(status==0) break;
            if(!authenticated) {
                if(frame.size()!=40 || frame.mid(4,4)!="NPW1" || !channel->connect(frame.mid(8).toStdString(),now())) {fail("Pairing rejected",ConnectionFailure::Pairing); return;}
                stream=frame.mid(8).toStdString();
                imagesSupported=frame.mid(8,4)=="ART1";
                if(imagesSupported&&!send(socket,packet("NPC1"))){fail("Image capability negotiation failed");return;}
                authenticated=true; retryAttempt=0; deadline.start(static_cast<int>(channel->timeoutSeconds()*1000)); notify();
            } else {
                if(frame.mid(4,4)=="NPI1") {if(!image(frame)){fail("Invalid image frame");return;}continue;}
                const std::string_view view(frame.constData(),static_cast<std::size_t>(frame.size()));
                if(channel->receive(view,now())!=net::Ingress::Accepted) {fail("Remote state rejected"); return;}
                deadline.start(static_cast<int>(channel->timeoutSeconds()*1000)); notify();
            }
        }
        const auto expected=socket;
        if(socket && (socket->bytesAvailable()>0 || buffer.size()>=maxPacket)) QTimer::singleShot(0,socket,[this,expected] {if(socket==expected) read();});
        else if(buffer.size()>=4) {
            QByteArray copy=buffer,frame; if(extract(copy,frame)>0) QTimer::singleShot(0,socket,[this,expected] {if(socket==expected) read();});
        }
    }
};
Receiver::Receiver(std::shared_ptr<net::Channel> channel,QObject *parent,QString root) : QObject(parent),impl_(std::make_unique<Impl>(this,std::move(channel),std::move(root))) {}
Receiver::~Receiver() {impl_->stop();}
bool Receiver::connectTo(const ReceiverSettings &settings) {
    owner(this); impl_->retry.stop(); impl_->retryAttempt=0; impl_->stop(); impl_->error.clear(); impl_->failure=ConnectionFailure::None; impl_->notify();
    if(settings.address.isEmpty() || settings.address.size()>255 || settings.address.contains(QChar::Null)
        || settings.port==0 || settings.certificateName.isEmpty() || settings.certificateName.size()>255
        || settings.certificateName.contains(QChar::Null) || settings.certificateSha256.size()!=32
        || !validCredential(settings.device) || !QSslSocket::supportsSsl()) {
        impl_->retrySettings.device.secret.fill('\0'); impl_->retrySettings={};
        impl_->error="Invalid connection settings"; impl_->failure=ConnectionFailure::InvalidSettings; impl_->notify(); return false;
    }
    impl_->retrySettings=settings; impl_->settings=settings; auto socket=new QSslSocket(this); impl_->socket=socket; socket->setReadBufferSize(maxPacket*2);
    auto config=QSslConfiguration::defaultConfiguration(); config.setProtocol(QSsl::TlsV1_2OrLater); config.setPeerVerifyMode(QSslSocket::VerifyPeer); socket->setSslConfiguration(config);
    QObject::connect(socket,&QSslSocket::sslErrors,this,[this,socket](const QList<QSslError> &errors) {
        if(impl_->socket!=socket) return;
        if(!impl_->pin()) {impl_->fail("Certificate pin mismatch",ConnectionFailure::CertificatePin); return;}
        for(const auto &error:errors) if(error.error()!=QSslError::SelfSignedCertificate
            && error.error()!=QSslError::SelfSignedCertificateInChain && error.error()!=QSslError::CertificateUntrusted) {
            impl_->fail("Certificate validation failed",ConnectionFailure::CertificateValidation); return;
        }
        socket->ignoreSslErrors(errors); // Only this matching pinned certificate's trust-chain errors.
    });
    QObject::connect(socket,&QSslSocket::encrypted,this,[this,socket] {
        if(impl_->socket!=socket) return;
        if(!impl_->pin()) {impl_->fail("Certificate pin mismatch",ConnectionFailure::CertificatePin); return;}
        const auto &device=impl_->settings.device;
        if(!send(socket,packet("NPA1"+device.id+device.secret))) impl_->fail("Pairing send failed");
    });
    QObject::connect(socket,&QSslSocket::readyRead,this,[this,socket] {if(impl_->socket==socket) impl_->read();});
    QObject::connect(socket,&QSslSocket::disconnected,this,[this,socket] {if(impl_->socket==socket) impl_->fail("Connection lost",impl_->authenticated?ConnectionFailure::Transport:ConnectionFailure::Pairing);});
    QObject::connect(socket,&QSslSocket::errorOccurred,this,[this,socket](QAbstractSocket::SocketError) {if(impl_->socket==socket) impl_->fail("Connection failed",socket->isEncrypted() && !impl_->authenticated?ConnectionFailure::Pairing:ConnectionFailure::Transport);});
    impl_->deadline.start(10000); socket->connectToHostEncrypted(settings.address,settings.port,settings.certificateName); return true;
}
void Receiver::stop() {owner(this); impl_->retry.stop(); impl_->retryAttempt=0; impl_->retrySettings.device.secret.fill('\0'); impl_->retrySettings={}; impl_->error.clear(); impl_->failure=ConnectionFailure::None; impl_->stop(); impl_->notify();}
bool Receiver::connected() const {owner(this); return impl_->authenticated;}
bool Receiver::connecting() const {owner(this); return impl_->socket && !impl_->authenticated;}
QString Receiver::lastError() const {owner(this); return impl_->error;}
ConnectionFailure Receiver::lastFailure() const {owner(this); return impl_->failure;}
void Receiver::setAutoReconnect(bool enabled) {owner(this); impl_->autoReconnect=enabled; if(!enabled) impl_->retry.stop(); impl_->notify();}
bool Receiver::reconnectPending() const {owner(this); return impl_->retry.isActive();}
} // namespace playing::companion
