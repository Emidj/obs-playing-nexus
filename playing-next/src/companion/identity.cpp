// SPDX-License-Identifier: MIT
#include "playing/companion/profile.hpp"
#include <windows.h>
#include <ncrypt.h>
#include <wincrypt.h>
#include <stdexcept>
namespace playing::companion {
namespace {
struct Key {NCRYPT_HANDLE value=0; ~Key() {if(value) NCryptFreeObject(value);}};
void require(bool ok) {if(!ok) throw std::runtime_error("Companion identity creation failed");}
}
ServerIdentity createIdentity() {
    Key provider,key;
    require(NCryptOpenStorageProvider(&provider.value,MS_KEY_STORAGE_PROVIDER,0)==ERROR_SUCCESS);
    require(NCryptCreatePersistedKey(provider.value,&key.value,NCRYPT_RSA_ALGORITHM,nullptr,0,NCRYPT_SILENT_FLAG)==ERROR_SUCCESS);
    DWORD bits=2048,policy=NCRYPT_ALLOW_EXPORT_FLAG|NCRYPT_ALLOW_PLAINTEXT_EXPORT_FLAG;
    require(NCryptSetProperty(key.value,NCRYPT_LENGTH_PROPERTY,reinterpret_cast<PBYTE>(&bits),sizeof(bits),0)==ERROR_SUCCESS);
    require(NCryptSetProperty(key.value,NCRYPT_EXPORT_POLICY_PROPERTY,reinterpret_cast<PBYTE>(&policy),sizeof(policy),0)==ERROR_SUCCESS);
    require(NCryptFinalizeKey(key.value,NCRYPT_SILENT_FLAG)==ERROR_SUCCESS);
    DWORD size=0; require(CertStrToNameW(X509_ASN_ENCODING,L"CN=OBS Playing Companion",CERT_X500_NAME_STR,nullptr,nullptr,&size,nullptr)!=0);
    QByteArray subject(static_cast<qsizetype>(size),'\0');
    require(CertStrToNameW(X509_ASN_ENCODING,L"CN=OBS Playing Companion",CERT_X500_NAME_STR,nullptr,reinterpret_cast<PBYTE>(subject.data()),&size,nullptr)!=0);
    CERT_NAME_BLOB name{size,reinterpret_cast<PBYTE>(subject.data())};
    CRYPT_ALGORITHM_IDENTIFIER algorithm{const_cast<char *>(szOID_RSA_SHA256RSA),{0,nullptr}};
    SYSTEMTIME start{},end{}; GetSystemTime(&start); end=start; end.wYear+=3;
    // Avoid an invalid February 29 in the expiry year.
    if(end.wMonth==2 && end.wDay==29) end.wDay=28;
    const auto certificate=CertCreateSelfSignCertificate(key.value,&name,CERT_CREATE_SELFSIGN_NO_KEY_INFO,nullptr,&algorithm,&start,&end,nullptr);
    require(certificate!=nullptr);
    const QByteArray der(reinterpret_cast<const char *>(certificate->pbCertEncoded),certificate->cbCertEncoded);
    CertFreeCertificateContext(certificate);
    require(NCryptExportKey(key.value,0,NCRYPT_PKCS8_PRIVATE_KEY_BLOB,nullptr,nullptr,0,&size,NCRYPT_SILENT_FLAG)==ERROR_SUCCESS && size<=16384);
    QByteArray privateDer(static_cast<qsizetype>(size),'\0');
    require(NCryptExportKey(key.value,0,NCRYPT_PKCS8_PRIVATE_KEY_BLOB,nullptr,reinterpret_cast<PBYTE>(privateDer.data()),size,&size,NCRYPT_SILENT_FLAG)==ERROR_SUCCESS);
    ServerIdentity identity{QSslCertificate(der,QSsl::Der),QSslKey(privateDer,QSsl::Rsa,QSsl::Der)};
    SecureZeroMemory(privateDer.data(),static_cast<SIZE_T>(privateDer.size()));
    require(!identity.certificate.isNull() && !identity.privateKey.isNull());
    return identity;
}
} // namespace playing::companion
