// SPDX-License-Identifier: MIT
#pragma once
#include "../visual/native-https.hpp"
#include <QObject>
#include <QThreadPool>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QDateTime>
#include <vector>
#include <map>
#include <functional>
namespace playing::obs {
// Uses the existing OBS-authenticated token, in memory only. No secret in scenes.
class TwitchProfiles final:public QObject {
    struct Entry{QUrl url;qint64 expires=0;bool pending=false;std::vector<std::function<void(QUrl)>> callbacks;};
    QThreadPool pool_;std::map<QString,Entry> cache_;
public:
    explicit TwitchProfiles(QObject *parent):QObject(parent){pool_.setMaxThreadCount(1);pool_.setExpiryTimeout(0);}
    ~TwitchProfiles() override{pool_.clear();pool_.waitForDone();}
    void invalidate(){for(auto &entry:cache_)entry.second.expires=0;}
    void request(QString channel,QByteArray token,std::function<void(QUrl)> done){
        if(token.isEmpty()||token.size()>4096||token.contains('\r')||token.contains('\n')){done({});return;}channel=channel.trimmed().toLower();
        if(!channel.isEmpty()&&!QRegularExpression("^[a-z0-9_]{1,25}$").match(channel).hasMatch()){done({});return;}
        const auto key=channel+":"+QString::fromLatin1(QCryptographicHash::hash(token,QCryptographicHash::Sha256).toHex());
        if(cache_.size()>16&&!cache_.count(key)){done({});return;}
        auto &entry=cache_[key];const auto now=QDateTime::currentMSecsSinceEpoch();if(!entry.pending&&entry.expires>now){done(entry.url);return;}
        entry.callbacks.push_back(std::move(done));if(entry.pending)return;entry.pending=true;
        pool_.start([this,key,channel,token]{QUrl image;
            const auto checked=visual::httpsGet(QUrl("https://id.twitch.tv/oauth2/validate"),"Authorization: OAuth "+QString::fromUtf8(token)+"\r\n",64*1024);
            const auto validation=QJsonDocument::fromJson(checked.body).object();const auto client=validation.value("client_id").toString();
            if(checked.status==200&&QRegularExpression("^[A-Za-z0-9]{1,128}$").match(client).hasMatch()){
                const auto query=channel.isEmpty()?"id="+validation.value("user_id").toString():"login="+channel;
                const auto reply=visual::httpsGet(QUrl("https://api.twitch.tv/helix/users?"+query),"Client-ID: "+client+"\r\nAuthorization: Bearer "+QString::fromUtf8(token)+"\r\n",64*1024);
                const auto users=QJsonDocument::fromJson(reply.body).object().value("data").toArray();
                if(reply.status==200&&users.size()==1&&(channel.isEmpty()?users[0].toObject().value("id")==validation.value("user_id"):users[0].toObject().value("login").toString()==channel)){const auto candidate=QUrl(users[0].toObject().value("profile_image_url").toString());
                    if(candidate.scheme()=="https"&&candidate.host()=="static-cdn.jtvnw.net"&&candidate.path().startsWith("/jtv_user_pictures/")&&candidate.userInfo().isEmpty()&&candidate.fragment().isEmpty()&&candidate.port(443)==443)image=candidate;}
            }
            QMetaObject::invokeMethod(this,[this,key,image]{auto &cached=cache_.at(key);cached.pending=false;cached.url=image;cached.expires=QDateTime::currentMSecsSinceEpoch()+(image.isEmpty()?60000:3600000);auto callbacks=std::move(cached.callbacks);for(const auto &callback:callbacks)callback(image);},Qt::QueuedConnection);
        });
    }
};
}
