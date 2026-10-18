// SPDX-License-Identifier: MIT
#pragma once
#include "transport.hpp"
#include <QHostAddress>
namespace playing::companion {
inline constexpr quint16 discoveryPort=39009;
struct DiscoveredDj {QString name,address; quint16 pairingPort=0,dataPort=0; QByteArray pin;};
QString pairingFingerprint(const QByteArray &pin); // 128 bits for visual comparison, no secret.
// Explicit two-minute window; advertisements contain no credentials.
class PairingServer final : public QObject {
public:
    explicit PairingServer(QObject *parent=nullptr); ~PairingServer() override;
    bool open(const ServerIdentity &,quint16 dataPort,const QString &name,
        const QHostAddress &bind=QHostAddress::AnyIPv4,quint16 discovery=discoveryPort,int lifetimeMs=120000);
    void stop(); bool active() const; quint16 discoverySocketPort() const;
    bool pending(const QByteArray &request) const;
    bool approve(const QByteArray &request,const DeviceCredential &credential);
    void reject(const QByteArray &request);
    std::function<void(QByteArray,QString)> requested; // Host confirms, persists and registers before approve.
    std::function<void()> changed;
private: struct Impl; std::unique_ptr<Impl> impl_;
};
class PairingClient final : public QObject {
public:
    explicit PairingClient(QObject *parent=nullptr); ~PairingClient() override;
    bool search(const QHostAddress &target=QHostAddress::Broadcast,quint16 port=discoveryPort);
    std::vector<DiscoveredDj> devices() const;
    bool request(const DiscoveredDj &,const QString &deviceName); // UI compares fingerprint first.
    void stop(); QString error() const; bool searching() const;
    std::function<void()> changed;
    std::function<void(ReceiverSettings)> completed;
private: struct Impl; std::unique_ptr<Impl> impl_;
};
} // namespace playing::companion
