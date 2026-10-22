// SPDX-License-Identifier: MIT
#pragma once
#include "playing/companion/detector.hpp"
#include "playing/companion/profile.hpp"
#include "playing/companion/pairing.hpp"
#include "playing/companion/protected-store.hpp"
#include <QWidget>
class QLabel; class QLineEdit; class QSpinBox; class QComboBox; class QCheckBox; class QListWidget; class QPushButton;
class QCloseEvent; class QMouseEvent; class QShowEvent;
class QDialog;
namespace playing::companion {
class Window final : public QWidget {
public:
    Window(QString profilePath,std::string databaseLibrary);
    ~Window() override;
protected:
    void closeEvent(QCloseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void showEvent(QShowEvent *event) override;
private:
    void fitToScreen();
    void applyDjSelection();
    void update();
    void stop();
    void save(const Profile &next);
    void setStartup(StartupMode mode);
    void guarded(const std::function<void()> &action);
    ProtectedStore store_;
    Profile profile_;
    providers::ProviderRegistry dj_,remote_;
    Sender sender_;
    Detector detector_;
    std::shared_ptr<providers::network::Channel> channel_;
    Receiver receiver_;
    PairingServer pairingServer_;
    PairingClient pairingClient_;
    providers::ConsumerToken remoteToken_;
    QLabel *status_=nullptr,*track_=nullptr,*detectorStatus_=nullptr;
    QDialog *quickSetup_=nullptr;
    QLabel *quickStatus_=nullptr,*quickIdentity_=nullptr;
    QLineEdit *address_=nullptr,*history_=nullptr,*code_=nullptr;
    QSpinBox *port_=nullptr;
    QComboBox *provider_=nullptr;
    QCheckBox *lan_=nullptr,*preview_=nullptr;
    QCheckBox *autoReconnect_=nullptr;
    QCheckBox *autoSend_=nullptr,*autoReceive_=nullptr;
    bool startupPending_=true;
    bool receiverView_=false;
    QLabel *clientsTitle_=nullptr,*linkedDj_=nullptr;
    QListWidget *devices_=nullptr,*pairedDevices_=nullptr;
    QLabel *artist_=nullptr,*artwork_=nullptr,*network_=nullptr,*endpoint_=nullptr,*count_=nullptr,*running_=nullptr;
    QWidget *bars_=nullptr;
    std::vector<QPushButton *> providerRows_;
    std::optional<core::ImageReference> displayedImage_;
};
} // namespace playing::companion
