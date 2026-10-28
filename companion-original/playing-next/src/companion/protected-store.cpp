// SPDX-License-Identifier: MIT
#include "playing/companion/protected-store.hpp"
#include <windows.h>
#include <dpapi.h>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QSaveFile>
#include <stdexcept>
#include <utility>
namespace playing::companion {
namespace {
constexpr qsizetype maxPlain=65536,maxCipher=262144;
struct Output : DATA_BLOB {
    Output() {cbData=0; pbData=nullptr;}
    ~Output() {if(pbData) {SecureZeroMemory(pbData,cbData); LocalFree(pbData);}}
};
QByteArray protect(const QByteArray &payload,bool decrypt) {
    DATA_BLOB input{static_cast<DWORD>(payload.size()),reinterpret_cast<BYTE *>(const_cast<char *>(payload.constData()))};
    static char purpose[]="OBSPlayingNext.Companion.v1";
    DATA_BLOB entropy{static_cast<DWORD>(sizeof(purpose)-1),reinterpret_cast<BYTE *>(purpose)};
    Output out;
    const auto ok=decrypt?CryptUnprotectData(&input,nullptr,&entropy,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&out)
        :CryptProtectData(&input,L"OBS Playing Next Companion",&entropy,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&out);
    if(!ok || out.cbData>static_cast<DWORD>(decrypt?maxPlain:maxCipher)) throw std::runtime_error("Protected profile operation failed");
    return QByteArray(reinterpret_cast<const char *>(out.pbData),out.cbData);
}
QByteArray read(const QString &path) {
    QFile file(path); if(!file.open(QIODevice::ReadOnly) || file.size()<8 || file.size()>maxCipher+8) throw std::runtime_error("Protected profile unreadable");
    const auto bytes=file.read(maxCipher+9); if(bytes.size()<8 || bytes.first(4)!="PCS1") throw std::runtime_error("Not a Companion protected profile");
    quint32 length=0; for(qsizetype i=4;i<8;++i) length=(length<<8)|static_cast<unsigned char>(bytes[i]);
    if(length==0 || length>static_cast<quint32>(maxCipher) || bytes.size()!=static_cast<qsizetype>(length)+8) throw std::runtime_error("Protected profile format invalid");
    return bytes.mid(8);
}
}
ProtectedStore::ProtectedStore(QString path) : path_(std::move(path)) {
    if(path_.isEmpty() || path_.size()>4096 || path_.contains(QChar::Null) || !QFileInfo(path_).isAbsolute()) throw std::invalid_argument("Protected profile needs absolute path");
}
void ProtectedStore::save(const QByteArray &payload) const {
    if(payload.size()>maxPlain) throw std::invalid_argument("Protected profile payload too large");
    QLockFile lock(path_+".lock"); if(!lock.tryLock(0)) throw std::runtime_error("Protected profile locked");
    if(QFileInfo::exists(path_)) protect(read(path_),true); // Refuse to overwrite unknown/corrupt data.
    const auto cipher=protect(payload,false); QByteArray envelope="PCS1";
    const auto length=static_cast<quint32>(cipher.size());
    for(unsigned i=0;i<4;++i) envelope+=static_cast<char>((length>>((3-i)*8))&255); envelope+=cipher;
    QSaveFile file(path_); file.setDirectWriteFallback(false);
    if(!file.open(QIODevice::WriteOnly) || file.write(envelope)!=envelope.size() || !file.commit()) throw std::runtime_error("Protected profile save failed");
}
std::optional<QByteArray> ProtectedStore::load() const {
    if(!QFileInfo::exists(path_)) return std::nullopt;
    QLockFile lock(path_+".lock"); if(!lock.tryLock(0)) throw std::runtime_error("Protected profile locked");
    return protect(read(path_),true);
}
} // namespace playing::companion
