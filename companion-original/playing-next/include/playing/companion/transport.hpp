// SPDX-License-Identifier: MIT
#pragma once
#include "playing/providers/network-provider.hpp"
#include <QByteArray>
#include <QObject>
#include <QSslCertificate>
#include <QSslKey>
#include <QString>
#include <functional>
#include <memory>
#include <vector>

namespace playing::companion {
struct DeviceCredential {
    QByteArray id; // 32 ASCII hex characters, host-generated.
    QByteArray secret; // 32 random bytes, never logged or sent outside TLS.
};
DeviceCredential newDeviceCredential();
QByteArray certificatePin(const QSslCertificate &certificate);
struct ServerIdentity { QSslCertificate certificate = QSslCertificate{}; QSslKey privateKey = QSslKey{}; };
struct ConnectedClient {QByteArray deviceId; QString address;};
// Owner Qt event-loop thread only. No OBS/renderer dependency, no DJ detection.
// Pairing UI/persistence must explicitly supply a trusted device credential.
class Sender final : public QObject {
public:
    explicit Sender(QObject *parent = nullptr);
    ~Sender() override;
    bool listen(const ServerIdentity &identity, const QString &bindAddress, quint16 port);
    quint16 port() const;
    bool pair(const DeviceCredential &credential);
    bool revoke(const QByteArray &deviceId);
    void publish(core::SnapshotPtr snapshot);
    std::size_t clientCount() const;
    std::vector<ConnectedClient> clients() const; // Authenticated only, no credentials.
    void stop();
    std::function<void()> clientsChanged;
private:
    struct Impl; std::unique_ptr<Impl> impl_;
};
struct ReceiverSettings {
    QString address;
    quint16 port = 0;
    QString certificateName = "OBS Playing Companion";
    QByteArray certificateSha256; // Pin from explicit out-of-band pairing.
    DeviceCredential device;
};
enum class ConnectionFailure {None, InvalidSettings, Transport, Timeout, CertificatePin, CertificateValidation, Pairing, Protocol};
class Receiver final : public QObject {
public:
    explicit Receiver(std::shared_ptr<providers::network::Channel> channel, QObject *parent = nullptr,
        QString artworkCacheRoot = {});
    ~Receiver() override;
    bool connectTo(const ReceiverSettings &settings);
    void stop();
    bool connected() const;
    bool connecting() const;
    QString lastError() const; // Generic; no tokens or payloads.
    ConnectionFailure lastFailure() const;
    void setAutoReconnect(bool enabled); // Explicit opt-in; transient failures only.
    bool reconnectPending() const;
    std::function<void()> changed; // Owner loop: host refreshes shared registry here.
private:
    struct Impl; std::unique_ptr<Impl> impl_;
};
} // namespace playing::companion
