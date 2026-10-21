// SPDX-License-Identifier: MIT
#include "chat-announcer.hpp"
#include "playing/companion/protected-store.hpp"
#include "playing/ui/i18n.hpp"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QTimer>
#include <QElapsedTimer>
#include <QRegularExpression>
#include <QFileInfo>
#include <QDir>
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDesktopServices>
#include <QPointer>
#include <QLockFile>
#include <stdexcept>
#include <algorithm>
#include <set>
namespace playing::obs {
namespace {
QString clean(QString text){
    text.replace(QRegularExpression("[\\x{0000}-\\x{001f}\\x{007f}-\\x{009f}]")," ");
    return text.normalized(QString::NormalizationForm_C).simplified();
}
QString trackKey(const ChatPolicy::Input &i){return QString::number(i.title.size())+":"+i.title+i.artist;}
constexpr auto client="xdxao5ubz0csvj28n0s70b7k3g0lnx"; // Existing public NEXUS app ID, not a secret.
bool token(const QString &s){return QRegularExpression("^[A-Za-z0-9_-]{1,4096}$").match(s).hasMatch();}
bool numeric(const QString &s){return QRegularExpression("^[0-9]{1,128}$").match(s).hasMatch();}
QByteArray form(std::initializer_list<std::pair<QString,QString>> values){QUrlQuery q;for(const auto &v:values)q.addQueryItem(v.first,v.second);return q.query(QUrl::FullyEncoded).toUtf8();}
}
void ChatPolicy::put(std::uint64_t id,std::optional<Input> input){
    if(input){input->title=clean(input->title);input->artist=clean(input->artist);
        if(input->title.isEmpty()||input->artist.isEmpty())input.reset();}
    if(input)inputs_[id]=std::move(*input);else inputs_.erase(id);
    if(inputs_.empty())candidate_.clear();
    else{const auto key=trackKey(inputs_.begin()->second);if(key!=candidate_)candidate_.clear();for(const auto &row:inputs_)if(trackKey(row.second)!=key)candidate_.clear();}
}
void ChatPolicy::live(bool value){if(value!=live_){candidate_.clear();attempted_.clear();lastAttempt_=-10000;}live_=value;}
bool ChatPolicy::pending()const{
    if(!live_||inputs_.empty())return false;
    const auto key=trackKey(inputs_.begin()->second);if(key==attempted_)return false;
    for(const auto &row:inputs_)if(trackKey(row.second)!=key)return false;
    return !message(inputs_.begin()->second).isEmpty();
}
QString ChatPolicy::message(const Input &i){
    auto format=clean(i.format).left(2048);
    if(!format.contains("{title}")||!format.contains("{artist}")||format.startsWith('/'))return {};
    // Substitute in one pass: metadata containing a placeholder is literal text.
    QString result;int from=0;const QRegularExpression placeholders("\\{(title|artist)\\}");auto matches=placeholders.globalMatch(format);
    while(matches.hasNext()){const auto match=matches.next();result+=format.mid(from,match.capturedStart()-from);result+=match.captured(1)=="title"?i.title:i.artist;from=int(match.capturedEnd());}result+=format.mid(from);
    const auto points=result.toUcs4();if(points.size()>500){result.clear();for(int index=0;index<499;++index){const char32_t point=points[index];result+=QString::fromUcs4(&point,1);}result+=QChar(0x2026);}
    return result.startsWith('/')?QString{}:result;
}
std::optional<QString> ChatPolicy::poll(qint64 now){
    if(!pending()){candidate_.clear();return {};}
    const auto &first=inputs_.begin()->second;const auto key=trackKey(first);int delay=first.delayMs;
    for(const auto &row:inputs_){if(trackKey(row.second)!=key){candidate_.clear();return {};}delay=std::max(delay,row.second.delayMs);}
    if(candidate_!=key){candidate_=key;since_=now;return {};}
    if(key==attempted_||now-since_<delay||now-lastAttempt_<10000)return {};
    const auto text=message(first);if(text.isEmpty())return {};
    attempted_=key;lastAttempt_=now;return text; // No blind retry after ambiguous POST failure.
}
struct ChatAnnouncer::Impl {
    struct Source{bool enabled=false;QString format;int delay=3000;core::SnapshotPtr snapshot;};
    ChatAnnouncer *owner;QString path;Endpoints endpoints;QNetworkAccessManager net;QTimer tick,validation,poll;
    QElapsedTimer clock,authClock;ChatPolicy policy;std::map<std::uint64_t,Source> sources;
    QString access,refresh,user,login,expected,device,userCode;QUrl verify;
    QString status=ui::string("Chat sin autorizar");
    std::function<void()> changed;std::set<QNetworkReply*> requests;QPointer<QNetworkReply> chatReply;
    std::uint64_t epoch=0;bool live=false,valid=false,busy=false,loading=false,storageOk=true;int interval=5,authSeconds=0;qint64 retryAt=0;
    std::unique_ptr<QLockFile> grantLease;
    bool authorizing=false;QPointer<QDialog> window;
    Impl(ChatAnnouncer *o,QString p,Endpoints e):owner(o),path(std::move(p)),endpoints(std::move(e)){
        clock.start();tick.setInterval(100);poll.setSingleShot(true);validation.setSingleShot(true);
        QObject::connect(&tick,&QTimer::timeout,owner,[this]{pump();});
        QObject::connect(&poll,&QTimer::timeout,owner,[this]{pollDevice();});
        QObject::connect(&validation,&QTimer::timeout,owner,[this]{if(demand())validate(true);});
    }
    ~Impl(){++epoch;for(auto *r:requests){QObject::disconnect(r,nullptr,owner,nullptr);r->abort();r->deleteLater();}}
    void report(const char *text){status=ui::string(text);if(changed)changed();}
    bool demand()const{for(const auto &row:sources)if(row.second.enabled)return true;return false;}
    void abortAll(){++epoch;poll.stop();validation.stop();busy=false;valid=false;device.clear();userCode.clear();verify={};auto old=requests;requests.clear();chatReply=nullptr;for(auto *r:old){QObject::disconnect(r,nullptr,owner,nullptr);r->abort();r->deleteLater();}}
    void request(QUrl url,QByteArray body,bool post,bool bearer,std::function<void(int,QByteArray)> done){
        QNetworkRequest req(url);req.setTransferTimeout(10000);req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
        if(bearer){req.setRawHeader("Authorization","Bearer "+access.toUtf8());req.setRawHeader("Client-Id",client);}
        else if(url.path().endsWith("/validate"))req.setRawHeader("Authorization","OAuth "+access.toUtf8());
        if(post)req.setHeader(QNetworkRequest::ContentTypeHeader,url==endpoints.chat?"application/json":"application/x-www-form-urlencoded");
        auto *reply=post?net.post(req,body):net.get(req);reply->setReadBufferSize(65537);requests.insert(reply);const auto generation=epoch;
        QObject::connect(reply,&QIODevice::readyRead,owner,[reply]{if(reply->bytesAvailable()>65536)reply->abort();});
        QObject::connect(reply,&QNetworkReply::finished,owner,[this,reply,generation,done=std::move(done)]{
            requests.erase(reply);const auto http=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();const auto bytes=reply->read(65537);reply->deleteLater();
            if(generation!=epoch)return;done(bytes.size()>65536?0:http,bytes.size()>65536?QByteArray{}:bytes);
        });
        if(url==endpoints.chat)chatReply=reply;
    }
    void load(){if(loading)return;loading=true;try{
        QDir().mkpath(QFileInfo(path).absolutePath());grantLease=std::make_unique<QLockFile>(path+".session.lock");if(!grantLease->tryLock(0))throw std::runtime_error("Chat grant already in use");
        const auto bytes=companion::ProtectedStore(path).load();if(!bytes)return;
        const auto o=QJsonDocument::fromJson(*bytes).object();if(o.value("kind")!="playing-chat-v1"||o.value("client")!=client)throw std::runtime_error("Unknown chat grant");
        access=o.value("access").toString();refresh=o.value("refresh").toString();if((!access.isEmpty()&&!token(access))||(!refresh.isEmpty()&&!token(refresh)))throw std::runtime_error("Invalid chat grant");
    }catch(...){storageOk=false;access.clear();refresh.clear();report("No se pudo leer la autorización protegida del chat.");}}
    bool save(){if(!storageOk)return false;try{QDir().mkpath(QFileInfo(path).absolutePath());companion::ProtectedStore(path).save(QJsonDocument(QJsonObject{{"kind","playing-chat-v1"},{"client",client},{"access",access},{"refresh",refresh}}).toJson(QJsonDocument::Compact));return true;}catch(...){storageOk=false;report("No se pudo guardar la autorización protegida del chat.");return false;}}
    void validate(bool allowRefresh){if(busy||access.isEmpty()||!demand())return;busy=true;
        request(endpoints.auth.resolved(QUrl("validate")),{},false,false,[this,allowRefresh](int http,QByteArray bytes){busy=false;
            if(http==401&&allowRefresh&&!refresh.isEmpty()){renew();return;}
            const auto o=QJsonDocument::fromJson(bytes).object();bool scope=false;for(const auto &s:o.value("scopes").toArray())if(s=="user:write:chat")scope=true;
            valid=http==200&&o.value("client_id")==client&&numeric(o.value("user_id").toString())&&QRegularExpression("^[a-z0-9_]{1,25}$").match(o.value("login").toString()).hasMatch()&&scope;
            if(!valid){report("Autoriza Twitch para enviar mensajes al chat.");schedule();return;}
            user=o.value("user_id").toString();login=o.value("login").toString();
            validation.start(std::clamp(o.value("expires_in").toInt()-60,30,3600)*1000);status=ui::string("Chat autorizado: %1").arg(login);if(changed)changed();schedule();
        });
    }
    void tokens(int http,const QByteArray &bytes){busy=false;const auto o=QJsonDocument::fromJson(bytes).object();
        if(http!=200||!token(o.value("access_token").toString())||!token(o.value("refresh_token").toString())){device.clear();userCode.clear();verify={};report("No se pudo autorizar el chat. Vuelve a intentarlo.");schedule();return;}
        authorizing=false;access=o.value("access_token").toString();refresh=o.value("refresh_token").toString();
        if(!save()){valid=false;return;}device.clear();userCode.clear();verify={};validate(false);
    }
    void renew(){if(busy||refresh.isEmpty())return;busy=true;valid=false;
        request(endpoints.auth.resolved(QUrl("token")),form({{"client_id",client},{"grant_type","refresh_token"},{"refresh_token",refresh}}),true,false,[this](int h,QByteArray b){tokens(h,b);});
    }
    void pollDevice(){if(device.isEmpty()||busy)return;if(authClock.elapsed()>=qint64(authSeconds)*1000){device.clear();userCode.clear();verify={};report("El código ha caducado. Pulsa Autorizar chat de Twitch.");return;}busy=true;
        request(endpoints.auth.resolved(QUrl("token")),form({{"client_id",client},{"scopes","user:write:chat"},{"device_code",device},{"grant_type","urn:ietf:params:oauth:grant-type:device_code"}}),true,false,[this](int h,QByteArray b){busy=false;
            const auto o=QJsonDocument::fromJson(b).object();const auto error=o.value("message").toString(o.value("error").toString());
            if(h==400&&(error=="authorization_pending"||error=="slow_down")){if(error=="slow_down")interval=std::min(60,interval+5);poll.start(interval*1000);return;}
            if(h==0){poll.start(interval*1000);return;}tokens(h,b);
        });
    }
    void input(std::uint64_t id){const auto &s=sources.at(id);const auto snap=s.snapshot;
        if(!s.enabled||!snap||!snap->running||snap->playback!=core::Playback::Playing||!snap->track||!core::validMetadata(*snap->track)||(snap->track->origin&&snap->track->origin->provider=="demo")){policy.put(id,{});return;}
        policy.put(id,ChatPolicy::Input{QString::fromUtf8(snap->track->title),QString::fromUtf8(snap->track->artist),s.format,s.delay});
    }
    void schedule(){if(valid&&live&&!expected.isEmpty()&&expected==login&&policy.pending()){if(!tick.isActive())tick.start();}else tick.stop();}
    void pump(){
        if(!live||!valid||busy||chatReply||expected.isEmpty()||expected!=login||clock.elapsed()<retryAt)return;
        const auto text=policy.poll(clock.elapsed());if(!text)return;
        schedule();
        report("Enviando canción al chat…");
        request(endpoints.chat,QJsonDocument(QJsonObject{{"broadcaster_id",user},{"sender_id",user},{"message",*text}}).toJson(QJsonDocument::Compact),true,true,[this](int h,QByteArray b){
            chatReply=nullptr;const auto data=QJsonDocument::fromJson(b).object().value("data").toArray();
            if(h==200&&data.size()==1&&data[0].toObject().value("is_sent").toBool())report("Canción enviada al chat.");
            else{if(h==401){valid=false;validation.start(1);}if(h==429)retryAt=clock.elapsed()+30000;report("Twitch no confirmó el envío. No se repetirá automáticamente.");}
        });
    }
};
ChatAnnouncer::ChatAnnouncer(QString path,QObject *parent):ChatAnnouncer(std::move(path),Endpoints{},parent){}
ChatAnnouncer::ChatAnnouncer(QString path,Endpoints endpoints,QObject *parent):QObject(parent),impl_(std::make_unique<Impl>(this,std::move(path),std::move(endpoints))){}
ChatAnnouncer::~ChatAnnouncer()=default;
void ChatAnnouncer::configure(std::uint64_t id,bool enabled,QString format,int delay){
    auto &s=impl_->sources[id];s.enabled=enabled;s.format=std::move(format);s.delay=std::clamp(delay,1000,30000);impl_->input(id);
    if(enabled){impl_->load();if(!impl_->valid&&!impl_->busy&&!impl_->access.isEmpty()&&impl_->device.isEmpty())impl_->validate(true);}
    if(!impl_->demand()){impl_->abortAll();impl_->loading=false;impl_->grantLease.reset();}impl_->schedule();
}
void ChatAnnouncer::observe(std::uint64_t id,core::SnapshotPtr snapshot){auto it=impl_->sources.find(id);if(it==impl_->sources.end())return;it->second.snapshot=std::move(snapshot);impl_->input(id);impl_->schedule();}
void ChatAnnouncer::remove(std::uint64_t id){impl_->sources.erase(id);impl_->policy.put(id,{});if(!impl_->demand()){impl_->abortAll();impl_->loading=false;impl_->grantLease.reset();}impl_->schedule();}
void ChatAnnouncer::broadcast(bool live,QString expected){
    expected=expected.trimmed().toLower();if(live!=impl_->live||expected!=impl_->expected){impl_->policy.live(false);if(impl_->chatReply)impl_->chatReply->abort();}
    impl_->live=live;impl_->expected=std::move(expected);impl_->policy.live(live);impl_->schedule();
    if(live&&impl_->valid&&(impl_->expected.isEmpty()||impl_->expected!=impl_->login))impl_->report("El chat autorizado no coincide con la cuenta de emisión de OBS.");
}
void ChatAnnouncer::authorize(){
    if(!impl_->demand())return;impl_->load();if(!impl_->storageOk)return;impl_->abortAll();impl_->authorizing=true;impl_->report("Preparando autorización de Twitch…");impl_->busy=true;
    impl_->request(impl_->endpoints.auth.resolved(QUrl("device")),form({{"client_id",client},{"scopes","user:write:chat"}}),true,false,[this](int h,QByteArray b){auto &p=*impl_;p.busy=false;
        const auto o=QJsonDocument::fromJson(b).object();const QUrl url(o.value("verification_uri").toString());const int seconds=o.value("expires_in").toInt(),interval=o.value("interval").toInt();
        if(h!=200||!token(o.value("device_code").toString())||!token(o.value("user_code").toString())||o.value("user_code").toString().size()>32||url.toEncoded().size()>2048||seconds<1||seconds>3600||interval<1||interval>60||url.scheme()!="https"||url.host()!="www.twitch.tv"||url.path()!="/activate"||!url.userInfo().isEmpty()||url.port(443)!=443||url.hasFragment()){
            p.report("No se pudo autorizar el chat. Vuelve a intentarlo.");return;}
        p.device=o.value("device_code").toString();p.userCode=o.value("user_code").toString();p.verify=url;p.interval=interval;p.authSeconds=seconds;p.authClock.start();p.poll.start(interval*1000);p.report("Abre Twitch e introduce este código para autorizar el chat.");
    });
}
void ChatAnnouncer::cancelAuthorization(){if(!impl_->authorizing)return;impl_->authorizing=false;impl_->abortAll();impl_->report("Chat sin autorizar");}
bool ChatAnnouncer::forget(){impl_->abortAll();impl_->access.clear();impl_->refresh.clear();const auto saved=impl_->save();impl_->report(saved?"Chat desconectado en este equipo.":"No se pudo guardar la autorización protegida del chat.");return saved;}
QString ChatAnnouncer::status()const{return impl_->status;}
QString ChatAnnouncer::code()const{return impl_->userCode;}
QUrl ChatAnnouncer::verification()const{return impl_->verify;}
bool ChatAnnouncer::authorized()const{return impl_->valid;}
bool ChatAnnouncer::scheduled()const{return impl_->tick.isActive();}
void ChatAnnouncer::setChanged(std::function<void()> cb){impl_->changed=std::move(cb);}
QDialog *ChatAnnouncer::dialog(QWidget *parent){
    if(impl_->window){impl_->window->show();impl_->window->raise();return impl_->window;}
    auto *d=new QDialog(parent);d->setAttribute(Qt::WA_DeleteOnClose);d->setWindowTitle(ui::string("Anuncios de canciones en Twitch"));d->resize(460,280);
    impl_->window=d;
    auto *layout=new QVBoxLayout(d);auto *help=new QLabel(ui::string("Autoriza la cuenta que utilizas para transmitir en Twitch. Solo se enviarán canciones reales mientras OBS esté transmitiendo."),d);help->setWordWrap(true);layout->addWidget(help);
    auto *state=new QLabel(d);state->setWordWrap(true);state->setObjectName("chatStatus");layout->addWidget(state);auto *codeLabel=new QLabel(d);codeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);codeLabel->setObjectName("chatCode");layout->addWidget(codeLabel);
    auto *start=new QPushButton(ui::string("Autorizar chat de Twitch"),d);start->setObjectName("chatAuthorize");layout->addWidget(start);
    auto *open=new QPushButton(ui::string("Abrir Twitch"),d);layout->addWidget(open);auto *forgetButton=new QPushButton(ui::string("Desconectar chat en este equipo"),d);layout->addWidget(forgetButton);
    auto *close=new QPushButton(ui::string("Cerrar"),d);layout->addWidget(close);
    auto *credit=new QLabel("a plugin from <a href=\"https://discord.gg/dhcqZPDYh\">DjEmiMas</a>",d);credit->setOpenExternalLinks(true);layout->addWidget(credit);
    auto *timer=new QTimer(d);QObject::connect(timer,&QTimer::timeout,d,[this,state,codeLabel,open]{state->setText(status());codeLabel->setText(code());open->setEnabled(!verification().isEmpty());});timer->start(250);state->setText(status());open->setEnabled(!verification().isEmpty());
    QObject::connect(start,&QPushButton::clicked,d,[this]{authorize();});QObject::connect(open,&QPushButton::clicked,d,[this]{const auto url=verification();if(!url.isEmpty())QDesktopServices::openUrl(url);});
    QObject::connect(forgetButton,&QPushButton::clicked,d,[this]{forget();});QObject::connect(close,&QPushButton::clicked,d,&QDialog::close);
    QObject::connect(d,&QDialog::finished,this,[this]{cancelAuthorization();});d->show();return d;
}
}
