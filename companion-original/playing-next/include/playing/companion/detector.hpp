// SPDX-License-Identifier: MIT
#pragma once
#include "transport.hpp"
#include <QTimer>
#include <vector>
namespace playing::companion {
enum class Detection { Inactive, Searching, Selected, Ambiguous, Error };
// Registry and Sender outlive this host; all use the same owner event-loop thread.
// No duplicate DJ readers: this class acquires existing provider factories.
class Detector final : public QObject {
public:
    Detector(providers::ProviderRegistry &registry, Sender &sender, QObject *parent = nullptr);
    ~Detector() override;
    bool select(std::optional<providers::Request> request); // null=Auto Detector.
    void setPreview(bool enabled);
    // Host calls after Sender.clientsChanged; no detection when preview is off
    // and no authenticated clients exist. No callback ownership is stolen.
    void syncDemand();
    Detection status() const;
    core::SnapshotPtr snapshot() const;
    std::vector<std::string> candidates() const;
    providers::ProviderIssue issue() const;
    std::function<void()> changed;
private:
    struct Session { providers::Request request; providers::ConsumerToken token; };
    void requireOwner() const;
    void rebuild();
    void release();
    void tick();
    void publish(core::SnapshotPtr snapshot, Detection status);
    providers::ProviderRegistry &registry_;
    Sender &sender_;
    QTimer timer_;
    core::PlayingCore empty_;
    core::SnapshotPtr snapshot_;
    std::optional<providers::Request> selection_;
    std::vector<Session> sessions_;
    std::vector<std::string> candidates_;
    bool preview_=false, active_=false;
    bool acquisitionFailed_=false;
    Detection status_=Detection::Inactive;
    providers::ProviderIssue issue_=providers::ProviderIssue::None;
};
// Fresh DJ registry only; factories remain inert until demand. DB path is an
// explicit trusted packaging dependency, not a DLL discovered through PATH.
bool registerNativeDetectors(providers::ProviderRegistry &registry, const std::string &databaseLibrary);
} // namespace playing::companion
