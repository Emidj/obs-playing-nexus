// SPDX-License-Identifier: MIT
#include "playing/companion/profile.hpp"
#include <QDateTime>
#include <QHostAddress>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <set>
#include <stdexcept>
namespace playing::companion {
namespace {
void require(bool ok) {if(!ok) throw std::invalid_argument("Companion profile or pairing format invalid");}
void keys(const QJsonObject &object,std::initializer_list<QString> allowed) {
    require(object.size()==static_cast<qsizetype>(allowed.size()));
    for(const auto &key:allowed) require(object.contains(key));
}
QString string(const QJsonObject &object,const QString &key,int max=4096) {
    const auto value=object.value(key); require(value.isString()); const auto s=value.toString();
    require(!s.isEmpty() && s.size()<=max && !s.contains(QChar::Null)); return s;
}
QByteArray bytes(const QJsonObject &object,const QString &key,int length=0) {
    const auto text=string(object,key,24000).toLatin1(); const auto b=QByteArray::fromBase64(text,QByteArray::AbortOnBase64DecodingErrors);
    require(!b.isEmpty() && b.toBase64()==text && (!length || b.size()==length)); return b;
}
void credential(const DeviceCredential &d) {
    require(d.id.size()==32 && d.secret.size()==32);
    for(const auto c:d.id) require((c>='0' && c<='9') || (c>='a' && c<='f'));
}
QJsonObject device(const DeviceCredential &d) {credential(d); return {{"id",QString::fromLatin1(d.id)},{"secret",QString::fromLatin1(d.secret.toBase64())}};}
DeviceCredential device(const QJsonObject &o) {
    keys(o,{"id","secret"}); DeviceCredential d{string(o,"id",32).toLatin1(),bytes(o,"secret",32)}; credential(d); return d;
}
void settings(const ReceiverSettings &s) {
    credential(s.device); require(s.port!=0 && s.certificateSha256.size()==32 && s.certificateName=="OBS Playing Companion");
    QHostAddress address; require(s.address.size()<=255 && address.setAddress(s.address) && !address.isNull() && address!=QHostAddress::Any && address!=QHostAddress::AnyIPv6 && address!=QHostAddress::AnyIPv4 && !address.isMulticast());
}
QJsonObject receiver(const ReceiverSettings &s) {
    settings(s); return {{"address",s.address},{"port",s.port},{"pin",QString::fromLatin1(s.certificateSha256.toBase64())},{"device",device(s.device)}};
}
ReceiverSettings receiver(const QJsonObject &o) {
    keys(o,{"address","port","pin","device"}); const auto port=o.value("port");
    require(port.isDouble() && port.toDouble()==port.toInt() && port.toInt()>0 && port.toInt()<=65535 && o.value("device").isObject());
    ReceiverSettings s{string(o,"address",255),static_cast<quint16>(port.toInt()),"OBS Playing Companion",bytes(o,"pin",32),device(o.value("device").toObject())}; settings(s); return s;
}
QJsonObject parse(const QByteArray &bytes,int max) {
    require(!bytes.isEmpty() && bytes.size()<=max); QJsonParseError error;
    const auto doc=QJsonDocument::fromJson(bytes,&error); require(error.error==QJsonParseError::NoError && doc.isObject()); return doc.object();
}
void identity(const ServerIdentity &i) {
    require(!i.certificate.isNull() && !i.privateKey.isNull() && i.privateKey.algorithm()==QSsl::Rsa && i.privateKey.length()>=2048);
    require(i.certificate.subjectInfo(QSslCertificate::CommonName)==QStringList{"OBS Playing Companion"});
    // Expiry is enforced by TLS. Preserve an expired profile for explicit renewal.
}
QJsonObject sender(const SenderPreferences &s) {
    QHostAddress address; require(s.port>=1024 && address.setAddress(s.address) && address.protocol()==QAbstractSocket::IPv4Protocol && !address.isNull() && address!=QHostAddress::AnyIPv4 && address!=QHostAddress::Broadcast && !address.isMulticast() && (s.localNetwork || address.isLoopback()));
    const auto provider=s.selection?QString::fromStdString(s.selection->providerId):"auto";
    const auto history=s.selection?QString::fromUtf8(s.selection->historyPath):QString{};
    require(provider=="auto" || provider=="virtualdj" || provider=="rekordbox" || provider=="serato" || provider=="traktor");
    if(s.selection) {
        require(s.selection->encoding==providers::HistoryEncoding::Automatic);
        require(s.selection->mode==providers::Mode::Automatic || s.selection->mode==providers::Mode::History);
        if(s.selection->mode==providers::Mode::Automatic) require(history.isEmpty());
        else require(provider!="serato" && provider!="traktor" && history.toUtf8().toStdString()==s.selection->historyPath && history.size()<=4096 && !history.contains(QChar::Null) && (history.isEmpty() || QFileInfo(history).isAbsolute()));
    }
    return {{"address",s.address},{"port",s.port},{"lan",s.localNetwork},{"provider",provider},{"mode",s.selection && s.selection->mode==providers::Mode::History?"history":"automatic"},{"history",history}};
}
SenderPreferences sender(const QJsonObject &o) {
    keys(o,{"address","port","lan","provider","mode","history"}); const auto port=o.value("port");
    require(port.isDouble() && port.toDouble()==port.toInt() && port.toInt()>=1024 && port.toInt()<=65535 && o.value("lan").isBool() && o.value("history").isString());
    const auto provider=string(o,"provider",32),mode=string(o,"mode",16); require(mode=="automatic" || mode=="history");
    SenderPreferences s{string(o,"address",255),static_cast<quint16>(port.toInt()),o.value("lan").toBool(),std::nullopt};
    if(provider!="auto") s.selection=providers::Request{provider.toStdString(),mode=="history"?providers::Mode::History:providers::Mode::Automatic,o.value("history").toString().toUtf8().toStdString()};
    else require(mode=="automatic" && o.value("history").toString().isEmpty());
    sender(s); return s;
}
}
QByteArray encodeProfile(const Profile &p) {
    require(p.startup==StartupMode::Manual || p.startup==StartupMode::Sender || p.startup==StartupMode::Receiver);
    require(p.startup!=StartupMode::Receiver || p.receiver.has_value());
    identity(p.identity); require(p.devices.size()<=16); QJsonArray devices; std::set<QByteArray> ids;
    if(p.streamerImage) require(core::validImageReference(*p.streamerImage) && p.streamerImage->kind==core::ImageKind::LocalFile);
    for(const auto &d:p.devices) {
        require(!d.name.trimmed().isEmpty() && d.name.size()<=80 && !d.name.contains(QChar::Null) && ids.insert(d.credential.id).second);
        devices.append(QJsonObject{{"name",d.name},{"credential",device(d.credential)}});
    }
    QJsonObject object{{"version",2},{"certificate",QString::fromLatin1(p.identity.certificate.toPem().toBase64())},{"key",QString::fromLatin1(p.identity.privateKey.toPem().toBase64())},{"devices",devices},{"receiver",p.receiver?QJsonValue(receiver(*p.receiver)):QJsonValue(QJsonValue::Null)},{"streamerImage",p.streamerImage?QJsonValue(QString::fromUtf8(p.streamerImage->location)):QJsonValue(QJsonValue::Null)},{"sender",sender(p.sender)},{"autoReconnect",p.autoReconnect}};
    object["version"]=3;
    object["startup"]=p.startup==StartupMode::Sender?"sender":p.startup==StartupMode::Receiver?"receiver":"manual";
    const auto result=QJsonDocument(object).toJson(QJsonDocument::Compact); require(result.size()<=65536); return result;
}
Profile decodeProfile(const QByteArray &payload) {
    const auto object=parse(payload,65536); const bool old=object.value("version")==QJsonValue(1);
    if(old) keys(object,{"version","certificate","key","devices","receiver","streamerImage","sender"});
    else if(object.value("version")==QJsonValue(2)) {keys(object,{"version","certificate","key","devices","receiver","streamerImage","sender","autoReconnect"}); require(object.value("autoReconnect").isBool());}
    else {keys(object,{"version","certificate","key","devices","receiver","streamerImage","sender","autoReconnect","startup"}); require(object.value("version")==QJsonValue(3) && object.value("autoReconnect").isBool());}
    require(object.value("devices").isArray() && object.value("sender").isObject());
    Profile p; p.identity={QSslCertificate(bytes(object,"certificate")),QSslKey(bytes(object,"key"),QSsl::Rsa)}; identity(p.identity);
    const auto list=object.value("devices").toArray(); require(list.size()<=16);
    for(const auto &entry:list) {require(entry.isObject()); const auto d=entry.toObject(); keys(d,{"name","credential"}); require(d.value("credential").isObject()); p.devices.push_back({string(d,"name",80),device(d.value("credential").toObject())});}
    const auto r=object.value("receiver"); require(r.isNull() || r.isObject()); if(r.isObject()) p.receiver=receiver(r.toObject());
    const auto image=object.value("streamerImage"); require(image.isNull() || image.isString()); if(image.isString()) p.streamerImage=core::ImageReference{core::ImageKind::LocalFile,string(object,"streamerImage").toUtf8().toStdString(),""};
    p.sender=sender(object.value("sender").toObject());
    p.autoReconnect=old?false:object.value("autoReconnect").toBool();
    if(object.value("version")==QJsonValue(3)) {
        const auto mode=string(object,"startup",16); require(mode=="manual" || mode=="sender" || mode=="receiver");
        p.startup=mode=="sender"?StartupMode::Sender:mode=="receiver"?StartupMode::Receiver:StartupMode::Manual;
    }
    encodeProfile(p); return p;
}
QString pairingCode(const ReceiverSettings &s) {
    return "PNXPAIR1."+QString::fromLatin1(QJsonDocument(receiver(s)).toJson(QJsonDocument::Compact).toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals));
}
ReceiverSettings parsePairingCode(const QString &code) {
    require(code.startsWith("PNXPAIR1.") && code.size()<=4096);
    const auto text=code.mid(9).toLatin1(); const auto b=QByteArray::fromBase64(text,QByteArray::Base64UrlEncoding|QByteArray::AbortOnBase64DecodingErrors);
    require(b.toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals)==text);
    return receiver(parse(b,3072));
}
} // namespace playing::companion
