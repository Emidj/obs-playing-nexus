// SPDX-License-Identifier: MIT
#pragma once
#include "playing/core/playing-core.hpp"
#include <QObject>
#include <QString>
#include <QUrl>
#include <map>
#include <memory>
#include <functional>
class QWidget;
class QDialog;
namespace playing::obs {
// GUI owner only. Deduplication concerns chat text, never authoritative Track IDs.
class ChatPolicy {
public:
    struct Input {QString title,artist,format;int delayMs=3000;};
    void put(std::uint64_t id,std::optional<Input> input);
    void live(bool value);
    bool pending() const;
    std::optional<QString> poll(qint64 now);
    static QString message(const Input &input);
private:
    std::map<std::uint64_t,Input> inputs_;
    QString candidate_,attempted_;
    qint64 since_=0,lastAttempt_=-10000;
    bool live_=false;
};
class ChatAnnouncer final:public QObject {
public:
    struct Endpoints {QUrl auth{"https://id.twitch.tv/oauth2/"},chat{"https://api.twitch.tv/helix/chat/messages"};};
    explicit ChatAnnouncer(QString storePath,QObject *parent=nullptr);
    // Injectable transport endpoints for local contract tests, never scene settings.
    ChatAnnouncer(QString storePath,Endpoints,QObject *parent=nullptr);
    ~ChatAnnouncer() override;
    void configure(std::uint64_t id,bool enabled,QString format,int delayMs);
    void observe(std::uint64_t id,core::SnapshotPtr snapshot);
    void remove(std::uint64_t id);
    void broadcast(bool twitchLive,QString expectedLogin);
    void authorize();
    void cancelAuthorization();
    bool forget();
    QDialog *dialog(QWidget *parent);
    QString status() const;
    QString code() const;
    QUrl verification() const;
    bool authorized() const;
    bool scheduled() const;
    void setChanged(std::function<void()> callback);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
