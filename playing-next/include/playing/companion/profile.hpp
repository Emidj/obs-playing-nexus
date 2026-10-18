// SPDX-License-Identifier: MIT
#pragma once
#include "transport.hpp"
#include <vector>
namespace playing::companion {
struct PairedDevice {QString name; DeviceCredential credential;};
struct SenderPreferences {
    QString address="127.0.0.1";
    quint16 port=9000;
    bool localNetwork=false;
    std::optional<providers::Request> selection;
};
enum class StartupMode {Manual, Sender, Receiver};
struct Profile {
    ServerIdentity identity;
    std::vector<PairedDevice> devices;
    std::optional<ReceiverSettings> receiver;
    std::optional<core::ImageReference> streamerImage;
    SenderPreferences sender;
    bool autoReconnect=false;
    StartupMode startup=StartupMode::Manual;
};
// Ephemeral Windows CNG key, exported only in memory. Host persists via DPAPI.
ServerIdentity createIdentity();
QByteArray encodeProfile(const Profile &profile);
Profile decodeProfile(const QByteArray &bytes); // Strict version/bounds, throws.
QString pairingCode(const ReceiverSettings &settings); // Secret; show only by user action.
ReceiverSettings parsePairingCode(const QString &code);
} // namespace playing::companion
