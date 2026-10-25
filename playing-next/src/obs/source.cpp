#include "playing/ui/i18n.hpp"
// SPDX-License-Identifier: MIT
#include <obs-module.h>
#include "nexus-now-playing-publisher.hpp"
#include "playing/visual/vertical-pro-geometry.hpp"
#include "preset-manager.hpp"
#include <QMessageBox>
#include <QPushButton>
#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#ifdef PLAYING_SOURCE_TEST_HOOKS
#include <QApplication>
#include <QInputDialog>
#include <QComboBox>
#include <QListWidget>
#include <QFileDialog>
#include <QJsonArray>
#include <util/platform.h>
#endif
#include <obs-frontend-api.h>
#include "playing/obs/session-host.hpp"
#include "playing/visual/obs-track-renderer.hpp"
#include "playing/visual/text-only-layout.hpp"
#include <QGuiApplication>
#include <QMetaObject>
#include <QThread>
#include <QTimer>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QPointer>
#include <QDialog>
#include <QSslSocket>
#include "receiver-link.hpp"
#include <mutex>
#include <algorithm>
#include <vector>
#include <cstring>
#include <QJsonDocument>
#include <QJsonObject>
#include "twitch-profile.hpp"
#include "chat-announcer.hpp"
#include "playing/companion/protected-store.hpp"
#include <util/config-file.h>
#ifdef PLAYING_SOURCE_TEST_HOOKS
#include "playing/companion/protected-store.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>
#endif
OBS_DECLARE_MODULE()
MODULE_EXPORT void obs_module_set_locale(const char *locale) {playing::ui::setLanguage(locale);}
MODULE_EXPORT void obs_module_free_locale(void) {}
MODULE_EXPORT const char *obs_module_description(void) { return "OBS Playing NEXUS"; }
namespace {
using namespace playing;
#ifdef PLAYING_SOURCE_TEST_HOOKS
struct RenderObservation {std::string title;std::uint64_t renderNs,videoNs;};
#endif
struct State {
    std::mutex mutex;
    providers::Request request;
    QString profilePath;
    QString twitchChannel;
    bool automaticProfileEnabled=true,profileRequested=false;
    unsigned profileFailures=0;
    std::uint64_t profileGeneration=0;
    bool manualProfile=false;
    QString profileStatus;
    bool chatEnabled=false;
    QString chatFormat;
    int chatDelayMs=3000;
    std::uint64_t chatId=0; // GUI-only identifier, unrelated to Track identity.
    std::optional<core::ImageReference> automaticProfile;
    bool showExisting=true;
    int hideAfterMs=0;
    visual::TrackLayout textLayout;
    std::string error;
    bool active=false,closed=false;
    bool retainAppearance=false;
    QByteArray configurationKey;
    core::TrackState demo;
    std::uint64_t revision=0;
    std::atomic<std::uint64_t> applied{0};
    std::shared_ptr<obs::Consumer> consumer=std::make_shared<obs::Consumer>();
    obs::SessionHost::Id id=0; // GUI only.
    providers::Request attached;
    bool listed=false;
#ifdef PLAYING_SOURCE_TEST_HOOKS
    std::atomic<bool> profileTrace{false};
    std::vector<RenderObservation> profileEvents; // State mutex; bounded128.
#endif
};
struct Source { std::shared_ptr<State> state=std::make_shared<State>();obs_source_t *source=nullptr; };
#ifdef PLAYING_SOURCE_TEST_HOOKS
struct RemoteFixture {companion::Sender sender;core::PlayingCore core;companion::DeviceCredential credential;};
#endif
struct Service {
    std::mutex dispatchMutex;
    QObject *dispatcher=nullptr;
    std::unique_ptr<obs::SessionHost> host;
    obs::TwitchProfiles *profiles=nullptr;
    obs::ChatAnnouncer *chat=nullptr;
    std::uint64_t chatSequence=0;
    std::mutex graphicsMutex;
    std::unique_ptr<oto::ObsVisualRenderer> renderer;
    std::unique_ptr<visual::TransitionSurface> transitionSurface;
    std::atomic<bool> closing{false};
    std::atomic<bool> streamStopping{false};
    std::vector<std::weak_ptr<State>> states; // GUI only.
    std::vector<QPointer<QDialog>> dialogs;
    std::string databaseLibrary;
    QString assetRoot;
    QString tlsPluginRoot;
    bool addedTlsPath=false;
#ifdef PLAYING_SOURCE_TEST_HOOKS
    std::unique_ptr<RemoteFixture> fixture;
#endif
    std::atomic<std::uint64_t> uploads{0},draws{0};
} service;
std::atomic<std::uint64_t> imageVersions{0};
visual::TrackLayout layout(int titleOverflow=1,int artistOverflow=1,int titleMinimum=12,int artistMinimum=12) {
    visual::TrackLayout value;
    value.title.box=QRect(12,8,600,46);value.title.pixelSize=30;
    value.artist.box=QRect(12,65,600,38);value.artist.pixelSize=22;
    value.artist.color=QColor(90,170,230);
    value.title.overflow=static_cast<oto::visual::Overflow>(titleOverflow);
    value.artist.overflow=static_cast<oto::visual::Overflow>(artistOverflow);
    value.title.minimumPixelSize=titleMinimum;value.artist.minimumPixelSize=artistMinimum;
    return value;
}
visual::TrackLayout configuredLayout(obs_data_t *settings) {
    const auto integer=[settings](const char *key,int low,int high){return static_cast<int>(std::clamp<long long>(obs_data_get_int(settings,key),low,high));};
    auto value=layout(integer("title_overflow",0,3),integer("artist_overflow",0,3),integer("title_minimum",8,30),integer("artist_minimum",8,22));
    value.titlePingPong=integer("title_overflow",0,4)==4;value.artistPingPong=integer("artist_overflow",0,4)==4;
    if(value.titlePingPong)value.title.overflow=oto::visual::Overflow::Clip;
    if(value.artistPingPong)value.artist.overflow=oto::visual::Overflow::Clip;
    value.animationProfile=0; // User removed Float/Breathe, including old saved settings.
    value.animationSpeed=integer("animation_speed",25,300);
    value.performanceProfile=1; // One fixed cadence; legacy saved profile values are ignored.
    try{visual::animation::program(value.animationProfile);}catch(const std::invalid_argument&){value.animationProfile=0;}
    value.normalPreset=integer("normal_preset",0,4096);
    const int proSelection=integer("pro_preset",0,4096);
    value.proPreset=proSelection==1?1:proSelection==4?2:proSelection==5?3:proSelection==6?4:proSelection==7?5:proSelection==8?6:proSelection==9?7:proSelection==10?8:proSelection==11?9:proSelection==12?10:proSelection==13?11:proSelection==14?12:proSelection==15?13:proSelection==16?14:0;
    value.proPanelOpacity=integer("pro_panel_opacity",0,100);value.proHue=integer("pro_hue",0,360);value.proLightIntensity=integer("pro_light_intensity",0,100);value.proLightPulse=obs_data_get_bool(settings,"pro_light_pulse");
    if(proSelection>=1&&proSelection<=16)value.normalPreset=0;
    if(value.normalPreset)try{visual::normal::presetFromSetting(value.normalPreset);}catch(const std::invalid_argument&){value.normalPreset=0;}
    value.normalTransition=static_cast<visual::normal::Transition>(integer("normal_transition",0,2));
    value.normalDuration=integer("normal_duration",200,2000);value.normalGap=integer("normal_gap",0,80);
    value.wholeTransition=obs_data_get_bool(settings,"transition_whole");
    value.normalSpeed=integer("normal_speed",10,150);value.normalPause=integer("normal_pause",0,10);
    value.canvas=QSize(integer("canvas_width",64,2048),integer("canvas_height",64,2048));
    const int textLayout=proSelection==2?1:proSelection==3?2:integer("text_layout",0,2);
    if(!value.normalPreset&&textLayout){if(!obs_data_has_user_value(settings,"canvas_width"))value.canvas.setWidth(textLayout==2?320:640);if(!obs_data_has_user_value(settings,"canvas_height"))value.canvas.setHeight(textLayout==2?360:120);}
    if(value.proPreset){if(!obs_data_has_user_value(settings,"canvas_width"))value.canvas.setWidth(value.proPreset==14?480:value.proPreset==13?384:value.proPreset==12?400:value.proPreset==11?416:value.proPreset==10?480:960);if(!obs_data_has_user_value(settings,"canvas_height"))value.canvas.setHeight(value.proPreset>=10?720:value.proPreset>=5?320:value.proPreset>=3?280:367);}
    if(value.normalPreset)value.canvas.setWidth(integer("canvas_width",400,1600));
    if(value.normalPreset)value.canvas.setHeight(visual::normal::normalPreviewHeight(value.normalPreset,value.canvas.width()));
    const auto box=[&integer,&value](const char *x,const char *y,const char *w,const char *h){
        const int left=integer(x,0,value.canvas.width()-1),top=integer(y,0,value.canvas.height()-1);
        return QRect(left,top,integer(w,1,value.canvas.width()-left),integer(h,1,value.canvas.height()-top));
    };
    value.title.box=box("title_x","title_y","title_width","title_height");
    value.artist.box=box("artist_x","artist_y","artist_width","artist_height");
    value.title.horizontal=static_cast<oto::visual::Horizontal>(integer("title_align",0,2));
    value.artist.horizontal=static_cast<oto::visual::Horizontal>(integer("artist_align",0,2));
    value.artworkEnabled=!value.normalPreset&&!(obs_data_has_user_value(settings,"preset_family")&&obs_data_get_int(settings,"preset_family")==0)&&obs_data_get_bool(settings,"artwork_enabled");value.profileFallback=obs_data_get_bool(settings,"artwork_fallback");
    value.artworkBox=box("artwork_x","artwork_y","artwork_width","artwork_height");
    const auto path=std::string(obs_data_get_string(settings,"streamer_image"));
    if(!path.empty())value.streamerProfile=core::ImageReference{path.rfind("https://",0)==0?core::ImageKind::Https:core::ImageKind::LocalFile,path,std::string(obs_data_get_string(settings,"streamer_image_version"))};
    for(int i=0;i<2;++i){auto *font=obs_data_get_obj(settings,i?"text_artist_font":"text_title_font");if(font){const auto flags=obs_data_get_int(font,"flags");auto &effect=value.textEffects[i];effect.underline=(flags&OBS_FONT_UNDERLINE)!=0;effect.strikeout=(flags&OBS_FONT_STRIKEOUT)!=0;obs_data_release(font);}}
    if(value.normalPreset){
        for(std::size_t i=0;i<2;++i){auto &override=value.normalText[i];const char *fontKey=i?"text_artist_font":"text_title_font",*colorKey=i?"text_artist_color":"text_title_color";
            if(obs_data_has_user_value(settings,fontKey)){auto *font=obs_data_get_obj(settings,fontKey);if(font){const auto family=QString::fromUtf8(obs_data_get_string(font,"face")).trimmed().left(128);if(!family.isEmpty())override.family=family;override.pixelSize=int(std::clamp<long long>(obs_data_get_int(font,"size"),8,128));const auto flags=obs_data_get_int(font,"flags");override.bold=(flags&OBS_FONT_BOLD)!=0;override.italic=(flags&OBS_FONT_ITALIC)!=0;obs_data_release(font);}}
            if(obs_data_has_user_value(settings,colorKey)){const auto color=uint32_t(obs_data_get_int(settings,colorKey));override.color=QColor(color&255,(color>>8)&255,(color>>16)&255,(color>>24)&255);}
        }
    }
    if(!value.normalPreset) {
        const auto style=[&](oto::visual::TextNode &node,const char *fontKey,const char *colorKey) {
            auto *font=obs_data_get_obj(settings,fontKey);
            if(font&&(textLayout||value.proPreset||obs_data_has_user_value(settings,fontKey))) {
                auto family=QString::fromUtf8(obs_data_get_string(font,"face")).trimmed().left(128);
                if(!family.isEmpty())node.family=family;
                node.pixelSize=int(std::clamp<long long>(obs_data_get_int(font,"size"),8,128));
                const auto flags=obs_data_get_int(font,"flags");node.bold=(flags&OBS_FONT_BOLD)!=0;node.italic=(flags&OBS_FONT_ITALIC)!=0;
            }
            if(font)obs_data_release(font);
            node.minimumPixelSize=std::min(node.minimumPixelSize,node.pixelSize);
            if(textLayout||value.proPreset||obs_data_has_user_value(settings,colorKey)){
                const auto color=uint32_t(obs_data_get_int(settings,colorKey));
                node.color=QColor(color&255,(color>>8)&255,(color>>16)&255,(color>>24)&255);
            }
        };
        style(value.title,"text_title_font","text_title_color");style(value.artist,"text_artist_font","text_artist_color");
        if(value.proPreset){
            if(!obs_data_has_user_value(settings,"text_title_font")){value.title.family="Bahnschrift SemiCondensed";value.title.pixelSize=56;value.title.bold=true;}
            if(!obs_data_has_user_value(settings,"text_artist_font")){value.artist.family="Bahnschrift SemiCondensed";value.artist.pixelSize=44;value.artist.bold=true;}
            if(value.proPreset==2&&!obs_data_has_user_value(settings,"text_artist_color"))value.artist.color=QColor(255,215,80);
            if(value.proPreset==3&&!obs_data_has_user_value(settings,"text_artist_color"))value.artist.color=QColor(145,160,184);if(value.proPreset==4&&!obs_data_has_user_value(settings,"text_artist_color"))value.artist.color=QColor(221,125,255);
        }
        if(value.proPreset==5){if(!obs_data_has_user_value(settings,"text_title_font"))value.title.family="Segoe Print";if(!obs_data_has_user_value(settings,"text_artist_font"))value.artist.family="Segoe Print";if(!obs_data_has_user_value(settings,"text_artist_color"))value.artist.color=QColor(255,216,35);}
        if((value.proPreset==6||value.proPreset==7||value.proPreset==9)&&!obs_data_has_user_value(settings,"text_artist_color"))value.artist.color=QColor(0,230,255);
        if(value.proPreset==8&&!obs_data_has_user_value(settings,"text_artist_color"))value.artist.color=QColor(255,0,180);
        if(textLayout||value.proPreset)value=visual::textOnlyLayout(value,!value.proPreset&&textLayout==2,12,12,16,512);
        if(value.proPreset){
            const int w=value.canvas.width(),h=value.canvas.height();
            const int margin=12;
            const int left=std::min(w-3,int(w*(value.proPreset==2?.078:value.proPreset==3?.06:value.proPreset==4?.086:value.proPreset==5?.0916:value.proPreset==6?.107:value.proPreset==7?.111:value.proPreset==8?.143:value.proPreset==9?.095:.116))),right=std::max(left+2,w-int(w*(value.proPreset==6?.13:value.proPreset==7?.10:value.proPreset==9?.15:.065))-margin);
            const int requested=std::max(w,h);
            const int size=std::max(1,std::min({requested,int(h*(value.proPreset==2?.57:value.proPreset==3?.72:value.proPreset==4?.68:value.proPreset==5?.594:value.proPreset==6?.54:value.proPreset==7?.58:value.proPreset==8?.65:value.proPreset==9?.66:.54)),int(w*(value.proPreset==2?.217:value.proPreset==3?.208:value.proPreset==4?.20:value.proPreset==5?.197:value.proPreset==6?.178:value.proPreset==7?.192:value.proPreset==8?.215:value.proPreset==9?.22:.217))}));
            const int coverTop=int(h*(value.proPreset==2?.219:value.proPreset==3?.166:value.proPreset==4?.16:value.proPreset==5?.264:value.proPreset==6?.238:value.proPreset==7?.222:value.proPreset==8?.179:value.proPreset==9?.19:.27));const int coverHeight=size;value.artworkBox=QRect(left,coverTop,size,std::min(coverHeight,h-coverTop));
            if(value.proPreset<=9)value.artworkBox=visual::horizontalCoverGeometry(value.proPreset,value.canvas).box;
            const int gap=64;
            const int textLeft=std::min(right-1,int(w*(value.proPreset==2?.44:value.proPreset==3?.37:value.proPreset==4?.32:value.proPreset==5?.365:value.proPreset==6?.345:value.proPreset==7?.38:value.proPreset==8?.435:value.proPreset==9?.37:.395))+margin+(value.artworkEnabled?gap-64:0));
            const int titleHeight=std::max(1,std::min(value.title.pixelSize*3/2,(h-12)/3));
            const int artistHeight=std::max(1,std::min(value.artist.pixelSize*3/2,(h-12)/3));
            const int lineGap=std::clamp(12,-std::min(128,h),h-titleHeight-artistHeight);
            const int relativeArtist=titleHeight+lineGap;
            const int blockStart=std::min(0,relativeArtist),blockHeight=std::max(titleHeight,relativeArtist+artistHeight)-blockStart;
            const int autoTop=std::min(h-blockHeight,(h-blockHeight)/2+(value.proPreset==6?18:0));
            obs_data_set_default_int(settings,"text_top_distance",autoTop);
            const int top=autoTop-blockStart;
            value.title.box=QRect(textLeft,top,right-textLeft,titleHeight);
            value.artist.box=QRect(textLeft,top+titleHeight+lineGap,right-textLeft,artistHeight);
        }
    }
    if(value.proPreset>=10){
        const int w=value.canvas.width(),h=value.canvas.height(),margin=12;
        if(!obs_data_has_user_value(settings,"text_title_font"))value.title.pixelSize=value.proPreset==13?28:value.proPreset>=11?32:36;
        if(!obs_data_has_user_value(settings,"text_artist_font"))value.artist.pixelSize=value.proPreset==13?26:value.proPreset>=11?28:30;
        if(!obs_data_has_user_value(settings,"text_artist_color"))value.artist.color=value.proPreset==14?QColor(0,230,255):value.proPreset==13?QColor(255,216,35):value.proPreset==12?QColor(255,215,80):value.proPreset==11?QColor(0,230,255):QColor(255,0,190);
        if(!obs_data_has_user_value(settings,"title_overflow"))value.title.overflow=oto::visual::Overflow::Wrap;
        if(value.proPreset==13){if(!obs_data_has_user_value(settings,"text_title_font"))value.title.family="Segoe Print";if(!obs_data_has_user_value(settings,"text_artist_font"))value.artist.family="Segoe Print";}
        value.title.horizontal=value.artist.horizontal=oto::visual::Horizontal::Center;
        const int requested=std::max(w,h);
        const int size=std::max(1,std::min({requested,int(w*(value.proPreset==14?.505:value.proPreset==13?.515:value.proPreset==12?.575:value.proPreset==11?.46:.415)),int(h*(value.proPreset==14?.337:value.proPreset==13?.28:value.proPreset==12?.32:value.proPreset==11?.266:.277))}));
        value.artworkBox=QRect(value.proPreset==13?int(w*.204):int(w*(value.proPreset==14?.486:value.proPreset>=11?.50:.515))-size/2,int(value.proPreset==14?h*.2514:value.proPreset==13?h*.235:value.proPreset==12?h*.22+w*.0425:h*(value.proPreset==11?.253:.275)),size,size);
        value.artworkBox=visual::proCoverGeometry(value.proPreset,value.canvas).box;
        int left=std::min(w-2,int(w*(value.proPreset==14?.18:value.proPreset==13?.16:.235))+margin),right=std::max(left+1,int(w*(value.proPreset==13?.90:.80))-margin);
        if(value.proPreset>=10&&value.proPreset<=12){
            const int width=std::min(right-left,w);
            const double centre=value.proPreset==12?w/2.:QRectF(value.artworkBox).center().x();
            left=std::clamp(qRound(centre-width/2.),0,w-width);right=left+width;
        }
        const int titleHeight=std::min(h/4,value.title.pixelSize*5/2),artistHeight=std::min(h/5,value.artist.pixelSize*3/2);
        const int gap=12,relative=titleHeight+gap;
        const int start=std::min(0,relative),block=std::max(titleHeight,relative+artistHeight)-start;
        const int imageGap=64;
        const int automatic=std::clamp(value.artworkBox.bottom()+1+imageGap-(value.proPreset==14?26:value.proPreset==13?12:40),0,h-block);
        const int top=automatic-start;
        obs_data_set_default_int(settings,"text_top_distance",automatic);
        value.title.box=QRect(left,top,right-left,titleHeight);value.artist.box=QRect(left,top+relative,right-left,artistHeight);
    }
    if(!value.normalPreset&&(value.proPreset||textLayout))value=visual::offsetTextBlock(value,integer("text_offset_x",-2048,2048),integer("text_offset_y",-2048,2048));
    for(std::size_t i=0;i<2;++i){const std::string prefix=i?"artist_":"title_";auto &effect=value.textEffects[i];effect.outline=integer((prefix+"outline_width").c_str(),0,8);effect.shadow=integer((prefix+"shadow_offset").c_str(),0,8);const auto color=[&](const char *suffix){const auto rgba=uint32_t(obs_data_get_int(settings,(prefix+suffix).c_str()));return QColor(rgba&255,(rgba>>8)&255,(rgba>>16)&255,(rgba>>24)&255);};effect.outlineColor=color("outline_color");effect.shadowColor=color("shadow_color");if(value.normalPreset)value.normalText[i].effects=effect;}
    return value;
}
void clearGpu() {
    // Lock order matches render: OBS graphics, then renderer mutex.
    obs_enter_graphics();
    {std::lock_guard<std::mutex> lock(service.graphicsMutex);service.transitionSurface.reset();service.renderer.reset();service.uploads=0;service.draws=0;}
    obs_leave_graphics();
}
void syncChatBroadcast(){
    if(!service.chat||service.closing)return;
    auto *stream=obs_frontend_get_streaming_service();auto *settings=stream?obs_service_get_settings(stream):nullptr;
    const auto name=QString::fromUtf8(settings?obs_data_get_string(settings,"service"):"");
    const bool twitch=name.startsWith("Twitch",Qt::CaseInsensitive);if(settings)obs_data_release(settings);
    auto *profile=obs_frontend_get_profile_config();const char *login=profile?config_get_string(profile,"Twitch","Name"):nullptr;
    service.chat->broadcast(twitch&&!service.streamStopping&&obs_frontend_streaming_active(),QString::fromUtf8(login?login:""));
}
void command(const std::shared_ptr<State> &state) noexcept {
    if(service.closing.load()) return;
    std::lock_guard<std::mutex> gate(service.dispatchMutex);
    if(service.closing.load()) return;
    if(!QMetaObject::invokeMethod(service.dispatcher,[state] {
        if(service.closing.load()) return;
        try {
            providers::Request request;QString profilePath;bool active=false,closed=false,showExisting=true;int hideAfterMs=0;std::uint64_t revision=0;core::TrackState demo;
            visual::TrackLayout textLayout;bool chatEnabled=false;QString chatFormat;int chatDelayMs=3000;
            {std::lock_guard<std::mutex> lock(state->mutex);request=state->request;profilePath=state->profilePath;active=state->active;closed=state->closed;revision=state->revision;showExisting=state->showExisting;hideAfterMs=state->hideAfterMs;textLayout=state->textLayout;demo=state->demo;chatEnabled=state->chatEnabled;chatFormat=state->chatFormat;chatDelayMs=state->chatDelayMs;}
            if(!closed&&state->id&&state->applied==revision)return;
            if(!state->chatId){state->chatId=++service.chatSequence;
                state->consumer->trackChanged=[weak=std::weak_ptr<State>(state)](core::SnapshotPtr snapshot){if(auto current=weak.lock();current&&service.chat&&!service.closing)service.chat->observe(current->chatId,std::move(snapshot));};}
            if(closed)service.chat->remove(state->chatId);
            else service.chat->configure(state->chatId,chatEnabled&&active,chatFormat,chatDelayMs);
            syncChatBroadcast();
            if(!service.host) service.host=std::make_unique<obs::SessionHost>(service.databaseLibrary,service.assetRoot);
            if(!state->listed) {service.states.push_back(state);state->listed=true;}
            if(state->id&&closed) {service.host->detach(state->id);state->id=0;}
            if(!closed) {
                if(request.providerId=="network"){
                    try{request.providerId=service.host->remoteProfile(profilePath);}
                    catch(...){if(state->id){service.host->detach(state->id);state->id=0;}throw;}
                }
                if(state->id&&(request<state->attached||state->attached<request)){service.host->detach(state->id);state->id=0;}
                if(!state->id) {state->id=request.providerId=="demo-preview"?service.host->attachDemo(demo,textLayout,state->consumer):service.host->attach(request,textLayout,state->consumer,showExisting,hideAfterMs);state->attached=request;}
                else service.host->configure(state->id,textLayout,showExisting,hideAfterMs);
                if(request.providerId=="demo-preview")service.host->demoTrack(state->id,demo);
                service.host->active(state->id,active);
                state->applied=revision;
                bool getProfile=false;QString channel;std::uint64_t profileGeneration=0;{std::lock_guard<std::mutex> lock(state->mutex);if(active&&textLayout.artworkEnabled&&textLayout.profileFallback&&state->automaticProfileEnabled&&!state->profileRequested&&!textLayout.streamerProfile){state->profileRequested=true;profileGeneration=++state->profileGeneration;channel=state->twitchChannel;state->profileStatus=playing::ui::string("Consultando imagen de Twitch…");getProfile=true;}}
                if(getProfile){
                    auto *profile=obs_frontend_get_profile_config();const char *stored=profile?config_get_string(profile,"Twitch","Token"):nullptr;
                    service.profiles->request(channel,stored?QByteArray(stored):QByteArray{},[weak=std::weak_ptr<State>(state),channel,profileGeneration](QUrl url,QString profileStatus){
                        const auto target=weak.lock();if(!target||service.closing)return;
                        int retryDelay=0;
                        {std::lock_guard<std::mutex> lock(target->mutex);
                         if(target->closed||target->profileGeneration!=profileGeneration||target->twitchChannel!=channel||!target->automaticProfileEnabled||target->manualProfile)return;
                         target->profileStatus=std::move(profileStatus);
                         if(url.isEmpty()){
                             if(!target->active){target->profileRequested=false;return;}
                             constexpr int delays[]={5000,15000,45000};
                             if(target->profileFailures>=3)return;
                             retryDelay=delays[target->profileFailures++];
                         }else{
                             target->profileFailures=0;
                             target->automaticProfile=core::ImageReference{core::ImageKind::Https,url.toEncoded().toStdString(),std::to_string(QDateTime::currentMSecsSinceEpoch())};
                             target->textLayout.streamerProfile=target->automaticProfile;++target->revision;
                         }}
                        if(!retryDelay){command(target);return;}
                        // Bounded recovery for a late OBS login or transient request failure.
                        // The weak state and generation cancel retries after removal/reload.
                        QTimer::singleShot(retryDelay,service.dispatcher,[weak,profileGeneration]{
                            const auto retry=weak.lock();if(!retry||service.closing)return;
                            {std::lock_guard<std::mutex> lock(retry->mutex);
                             if(retry->closed||retry->profileGeneration!=profileGeneration||!retry->automaticProfileEnabled||retry->manualProfile||retry->automaticProfile)return;
                             retry->profileRequested=false;
                             if(!retry->active)return;
                             ++retry->revision;}
                            command(retry);
                        });
                    });
                }
                {std::lock_guard<std::mutex> lock(state->mutex);state->error.clear();}
            }
            if(!service.host->consumers()) clearGpu();
            service.states.erase(std::remove_if(service.states.begin(),service.states.end(),[](const auto &entry){return entry.expired();}),service.states.end());
        } catch(const std::exception &error) {
            state->consumer->publication.publish({});
            {std::lock_guard<std::mutex> lock(state->mutex);state->error=error.what();}
            blog(LOG_WARNING,"[playing-next] Source configuration failed: %s",error.what());
        }
    },Qt::QueuedConnection)) blog(LOG_WARNING,"[playing-next] GUI command unavailable");
}
void update(void *data,obs_data_t *settings) noexcept {
    auto state=static_cast<Source *>(data)->state;
    auto object=QJsonDocument::fromJson(QByteArray(obs_data_get_json(settings))).object();
    object.remove("editor_mode"); // UI preference; never invalidate visible pixels.
    object.remove("performance_profile"); // Consumer scheduling; appearance unchanged.
    for(const auto key:{"chat_enabled","chat_message","chat_delay"})object.remove(key);
    for(const auto key:{"preset_manager_id","pro_hue","pro_light_intensity","pro_light_pulse","pro_panel_opacity","animation_speed","demo_title","demo_artist","demo_index","preset_family","preset_style","text_top_distance","text_line_gap","text_offset_x","text_offset_y"})object.remove(key);
    for(const auto key:{"text_title_font","text_artist_font","text_title_color","text_artist_color","title_outline_width","title_outline_color","title_shadow_offset","title_shadow_color","artist_outline_width","artist_outline_color","artist_shadow_offset","artist_shadow_color"})object.remove(key);
    const auto key=QJsonDocument(object).toJson(QJsonDocument::Compact);
    {std::lock_guard<std::mutex> lock(state->mutex);
     state->retainAppearance=!state->configurationKey.isEmpty()&&key==state->configurationKey;
     state->configurationKey=key;
     state->chatEnabled=obs_data_get_bool(settings,"chat_enabled");
     state->chatFormat=QString::fromUtf8(obs_data_get_string(settings,"chat_message")).left(2048);
     state->chatDelayMs=static_cast<int>(std::clamp<long long>(obs_data_get_int(settings,"chat_delay"),1,30))*1000;
     state->request={obs_data_get_string(settings,"provider"),obs_data_get_int(settings,"mode")==1?providers::Mode::History:providers::Mode::Automatic,
         obs_data_get_string(settings,"history")};
     if(state->request.providerId=="serato")state->request.mode=providers::Mode::Automatic;
     if(state->request.mode==providers::Mode::Automatic)state->request.historyPath.clear();
     const auto encoding=obs_data_get_int(settings,"encoding");
     state->request.encoding=encoding==1?providers::HistoryEncoding::Ansi:encoding==2?providers::HistoryEncoding::Utf16LE:providers::HistoryEncoding::Automatic;
     if(state->request.providerId!="virtualdj"||state->request.mode==providers::Mode::Automatic)state->request.encoding=providers::HistoryEncoding::Automatic;
     if(state->request.providerId=="network"||state->request.providerId=="serato"||state->request.providerId=="traktor"){state->request.mode=providers::Mode::Automatic;state->request.historyPath.clear();state->request.encoding=providers::HistoryEncoding::Automatic;}
     state->profilePath=QString::fromUtf8(obs_data_get_string(settings,"receiver_profile"));
     state->showExisting=obs_data_get_bool(settings,"show_existing");
     state->hideAfterMs=static_cast<int>(std::clamp<long long>(obs_data_get_int(settings,"hide_seconds"),0,3600))*1000;
     if(obs_data_get_bool(settings,"demo_enabled")){
         state->request={"demo-preview",providers::Mode::Automatic,{}};state->showExisting=true;state->hideAfterMs=0;
         state->demo={QString::fromUtf8(obs_data_get_string(settings,"demo_title")).left(1024).toUtf8().toStdString(),QString::fromUtf8(obs_data_get_string(settings,"demo_artist")).left(1024).toUtf8().toStdString()};
         const auto demoImage=std::string(obs_data_get_string(settings,"demo_image"));
         if(!demoImage.empty())state->demo.artwork={core::ArtworkStatus::Ready,core::ImageReference{core::ImageKind::LocalFile,demoImage,{}}};
     }
     const auto channel=QString::fromUtf8(obs_data_get_string(settings,"twitch_channel")).trimmed().left(25);const bool autoProfile=obs_data_get_bool(settings,"twitch_profile_auto");
     if(state->twitchChannel!=channel||state->automaticProfileEnabled!=autoProfile){state->automaticProfile.reset();state->profileRequested=false;state->profileFailures=0;++state->profileGeneration;}
     state->twitchChannel=channel;state->automaticProfileEnabled=autoProfile;
     auto nextLayout=configuredLayout(settings);
     state->manualProfile=nextLayout.streamerProfile.has_value();
     if(!nextLayout.streamerProfile&&autoProfile&&state->automaticProfile)nextLayout.streamerProfile=state->automaticProfile;
     if(nextLayout.streamerProfile&&state->textLayout.streamerProfile&&nextLayout.streamerProfile->location==state->textLayout.streamerProfile->location&&nextLayout.streamerProfile->version.empty())nextLayout.streamerProfile->version=state->textLayout.streamerProfile->version;
     state->textLayout=std::move(nextLayout);
     ++state->revision;}
    // Revision gates rendering until the GUI applies this immutable settings copy.
    command(state);
}
void *create(obs_data_t *settings,obs_source_t *obsSource) noexcept {
    try {auto source=std::make_unique<Source>();source->source=obsSource;update(source.get(),settings);nexus_now_playing::attach<Source>(obsSource,source.get());return source.release();}
    catch(...) {blog(LOG_ERROR,"[playing-next] Source creation failed");return nullptr;}
}
void destroy(void *data) noexcept {
    auto source=static_cast<Source *>(data);auto state=source->state;
    {std::lock_guard<std::mutex> lock(state->mutex);state->closed=true;state->active=false;}
    state->consumer->publication.publish({});command(state);delete source;
}
void activity(void *data,bool value) noexcept {
    auto state=static_cast<Source *>(data)->state;
    {std::lock_guard<std::mutex> lock(state->mutex);state->active=value;++state->revision;}
    if(!value) state->consumer->publication.publish({});
    command(state);
}
void activate(void *data) {activity(data,true);}void deactivate(void *data) {activity(data,false);}
void render(void *data,gs_effect_t *) noexcept {
    if(service.closing.load())return;
    auto state=static_cast<Source *>(data)->state;
    {std::lock_guard<std::mutex> lock(state->mutex);if(!state->active||state->closed||(state->revision!=state->applied.load()&&!state->retainAppearance))return;}
    const auto frame=state->consumer->publication.read();if(!frame)return;
    try {
        std::lock_guard<std::mutex> lock(service.graphicsMutex);
        if(service.closing.load())return;
        if(!service.renderer)service.renderer=std::make_unique<oto::ObsVisualRenderer>();
        if(!service.transitionSurface)service.transitionSurface=std::make_unique<visual::TransitionSurface>();
        visual::drawTrack(*service.renderer,frame,true,service.transitionSurface.get());
        service.uploads=service.renderer->stats().uploads;service.draws=service.renderer->stats().draws;
#ifdef PLAYING_SOURCE_TEST_HOOKS
        if(state->profileTrace.load(std::memory_order_relaxed)&&frame->snapshot&&frame->snapshot->track&&frame->normalSample.transition.alpha>=.05){
            std::lock_guard<std::mutex> traceLock(state->mutex);
            const auto &title=frame->snapshot->track->title;
            if(state->profileEvents.size()<128&&(state->profileEvents.empty()||state->profileEvents.back().title!=title))
                state->profileEvents.push_back({title,os_gettime_ns(),obs_get_video_frame_time()});
        }
#endif
    }catch(const std::exception &error){blog(LOG_ERROR,"[playing-next] Render failed: %s",error.what());}
}
const char *name(void *) {return "OBS Playing NEXUS";}
uint32_t width(void *data) {auto state=static_cast<Source *>(data)->state;std::lock_guard<std::mutex> lock(state->mutex);const auto frame=state->consumer->publication.read();if(frame&&(state->revision==state->applied.load()||state->retainAppearance))return frame->canvas.width();return state->textLayout.canvas.width();}
uint32_t height(void *data) {auto state=static_cast<Source *>(data)->state;std::lock_guard<std::mutex> lock(state->mutex);const auto frame=state->consumer->publication.read();if(frame&&(state->revision==state->applied.load()||state->retainAppearance))return frame->canvas.height();return state->textLayout.normalPreset?visual::normal::normalPreviewHeight(state->textLayout.normalPreset,state->textLayout.canvas.width()):state->textLayout.canvas.height();}
void defaults(obs_data_t *settings) {
    obs_data_set_default_int(settings,"editor_mode",0);
    obs_data_set_default_int(settings,"performance_profile",1);
    obs_data_set_default_string(settings,"demo_title","Night Drive (Original Mix)");obs_data_set_default_string(settings,"demo_artist","Neon Avenue");
    obs_data_set_default_int(settings,"animation_speed",100);
    obs_data_set_default_int(settings,"pro_light_intensity",65);obs_data_set_default_int(settings,"pro_panel_opacity",100);
    // Repeated source creation can share one settings object. Retain an existing
    // default font: libobs default-only object replacement does not add a ref
    // on the affected runtimes, leaving the replacement dangling after release.
    for(const auto &font:{std::pair<const char *,int>{"text_title_font",30},{"text_artist_font",22}}){if(obs_data_has_default_value(settings,font.first))continue;auto *value=obs_data_create();obs_data_set_string(value,"face","Segoe UI");obs_data_set_int(value,"size",font.second);obs_data_set_int(value,"flags",0);obs_data_set_default_obj(settings,font.first,value);obs_data_release(value);}
    obs_data_set_default_int(settings,"text_title_color",0xffffffff);obs_data_set_default_int(settings,"text_artist_color",0xffe6aa5a);
    for(const auto prefix:{std::string("title_"),std::string("artist_")}){obs_data_set_default_int(settings,(prefix+"outline_width").c_str(),0);obs_data_set_default_int(settings,(prefix+"shadow_offset").c_str(),0);obs_data_set_default_int(settings,(prefix+"outline_color").c_str(),0xff000000);obs_data_set_default_int(settings,(prefix+"shadow_color").c_str(),0xa0000000);}
    obs_data_set_default_int(settings,"text_offset_x",0);obs_data_set_default_int(settings,"text_offset_y",0);
    obs_data_set_default_int(settings,"text_margin",12);obs_data_set_default_int(settings,"text_line_gap",12);obs_data_set_default_int(settings,"text_image_gap",16);obs_data_set_default_int(settings,"text_cover_size",192);
    obs_data_set_default_bool(settings,"artwork_fallback",true);
    obs_data_set_default_bool(settings,"twitch_profile_auto",true);
    obs_data_set_default_bool(settings,"chat_enabled",false);
    obs_data_set_default_string(settings,"chat_message","🎵 Now playing: {artist} — {title}");
    obs_data_set_default_int(settings,"chat_delay",3);
    obs_data_set_default_int(settings,"artwork_width",100);obs_data_set_default_int(settings,"artwork_height",100);
    obs_data_set_default_int(settings,"normal_duration",800);obs_data_set_default_int(settings,"normal_gap",12);
    obs_data_set_default_int(settings,"normal_speed",40);obs_data_set_default_int(settings,"normal_pause",2);
    obs_data_set_default_int(settings,"canvas_width",640);obs_data_set_default_int(settings,"canvas_height",120);
    obs_data_set_default_int(settings,"title_x",12);obs_data_set_default_int(settings,"title_y",8);
    obs_data_set_default_int(settings,"title_width",600);obs_data_set_default_int(settings,"title_height",46);
    obs_data_set_default_int(settings,"artist_x",12);obs_data_set_default_int(settings,"artist_y",65);
    obs_data_set_default_int(settings,"artist_width",600);obs_data_set_default_int(settings,"artist_height",38);
    obs_data_set_default_bool(settings,"show_existing",true);
    obs_data_set_default_int(settings,"title_overflow",1);obs_data_set_default_int(settings,"artist_overflow",1);
    obs_data_set_default_int(settings,"title_minimum",12);obs_data_set_default_int(settings,"artist_minimum",12);
    obs_data_set_default_string(settings,"provider","virtualdj");obs_data_set_default_int(settings,"mode",1);
    const auto path=QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)).filePath("OBS Playing Next/OBS Receiver/profile.pcs").toUtf8();
    obs_data_set_default_string(settings,"receiver_profile",path.constData());
}
bool visibility(obs_properties_t *props,obs_property_t *,obs_data_t *settings) {
    const char *advanced[]={"title_outline_width","title_outline_color","title_shadow_offset","title_shadow_color","artist_outline_width","artist_outline_color","artist_shadow_offset","artist_shadow_color","title_overflow","artist_overflow","title_minimum","artist_minimum","title_align","artist_align","normal_transition","normal_duration","normal_speed","normal_pause","transition_whole"};
    const char *layout[]={"canvas_width","canvas_height","text_layout","text_offset_x","text_offset_y","text_margin","text_line_gap","text_top_distance","text_image_gap","text_cover_size","normal_gap","artwork_x","artwork_y","artwork_width","artwork_height","title_x","title_y","artist_x","artist_y","title_width","title_height","artist_width","artist_height"};
    for(const auto key:advanced)obs_property_set_visible(obs_properties_get(props,key),true);
    for(const auto key:layout)obs_property_set_visible(obs_properties_get(props,key),true);
    const auto proSelection=obs_data_get_int(settings,"pro_preset");
    const bool pro=proSelection==1||proSelection==4||proSelection==5||proSelection==6||proSelection==7||proSelection==8||proSelection==9||proSelection==10||proSelection==11||proSelection==12||proSelection==13||proSelection==14||proSelection==15||proSelection==16,proText=proSelection==2||proSelection==3;
    const bool normal=!pro&&!proText&&obs_data_get_int(settings,"normal_preset")!=0;
    const bool automatic=!normal&&(pro||proText||obs_data_get_int(settings,"text_layout")!=0),cover=!normal&&obs_data_get_bool(settings,"artwork_enabled");
    obs_property_set_enabled(obs_properties_get(props,"normal_preset"),true);
    for(const auto key:{"artwork_enabled","artwork_fallback","choose_streamer_image","manual_profile_url","clear_streamer_image","reload_artwork","twitch_profile_auto","twitch_channel"})obs_property_set_visible(obs_properties_get(props,key),!normal);
    for(const auto key:{"pro_hue","pro_light_intensity","pro_light_pulse","pro_panel_opacity"})obs_property_set_visible(obs_properties_get(props,key),pro);
    obs_property_set_visible(obs_properties_get(props,"text_layout"),!normal&&!pro&&!proText);
    for(const auto key:{"text_title_font","text_artist_font","text_title_color","text_artist_color"})obs_property_set_visible(obs_properties_get(props,key),true);
    for(const auto key:{"text_margin","text_line_gap"})obs_property_set_visible(obs_properties_get(props,key),automatic);
    obs_property_set_visible(obs_properties_get(props,"text_top_distance"),false);
    if(pro){const auto geometry=configuredLayout(settings);const int blockHeight=std::max(geometry.title.box.bottom(),geometry.artist.box.bottom())-std::min(geometry.title.box.top(),geometry.artist.box.top())+1;obs_property_int_set_limits(obs_properties_get(props,"text_top_distance"),0,std::max(0,geometry.canvas.height()-blockHeight),1);obs_property_int_set_limits(obs_properties_get(props,"text_line_gap"),-std::min(128,geometry.canvas.height()),std::min(128,geometry.canvas.height()-geometry.title.box.height()-geometry.artist.box.height()),1);}else obs_property_int_set_limits(obs_properties_get(props,"text_line_gap"),0,128,1);
    for(const auto key:{"text_margin","text_line_gap","text_top_distance","text_image_gap","text_cover_size"})obs_property_set_visible(obs_properties_get(props,key),false);
    for(const auto key:{"text_offset_x","text_offset_y"})obs_property_set_visible(obs_properties_get(props,key),automatic);
    if(automatic){const auto geometry=configuredLayout(settings);const int x=geometry.canvas.width()/2,y=geometry.canvas.height()/2;
        obs_property_int_set_limits(obs_properties_get(props,"text_offset_x"),-x,x,1);obs_property_int_set_limits(obs_properties_get(props,"text_offset_y"),-y,y,1);}
    for(const auto key:{"artwork_x","artwork_y","artwork_width","artwork_height"})obs_property_set_visible(obs_properties_get(props,key),cover&&!automatic);
    for(const auto key:{"normal_gap","transition_whole"})obs_property_set_visible(obs_properties_get(props,key),normal);
    const bool pingpong=obs_data_get_int(settings,"title_overflow")==4||obs_data_get_int(settings,"artist_overflow")==4;
    for(const auto key:{"normal_speed","normal_pause"})obs_property_set_visible(obs_properties_get(props,key),normal||pingpong);
    for(const auto key:{"canvas_height","title_overflow","artist_overflow","title_x","title_y","artist_x","artist_y","title_width","title_height","artist_width","artist_height","title_align","artist_align"})obs_property_set_visible(obs_properties_get(props,key),!normal);
    for(const auto key:{"title_x","title_y","artist_x","artist_y","title_width","title_height","artist_width","artist_height"})obs_property_set_visible(obs_properties_get(props,key),!normal&&!automatic);
    const std::string provider=obs_data_get_string(settings,"provider");
    obs_property_set_visible(obs_properties_get(props,"manual_profile_url"),!normal);
    obs_property_set_visible(obs_properties_get(props,"clear_streamer_image"),!normal&&obs_data_get_string(settings,"streamer_image")[0]!=0);
    const bool demo=obs_data_get_bool(settings,"demo_enabled");
    obs_property_set_visible(obs_properties_get(props,"demo_image"),demo&&!normal);
    obs_property_set_enabled(obs_properties_get(props,"provider"),!demo);
    const bool history=provider=="virtualdj"||provider=="rekordbox",remote=provider=="network";
    if(!history)obs_data_set_int(settings,"mode",0);
    obs_property_set_visible(obs_properties_get(props,"mode"),true);obs_property_set_enabled(obs_properties_get(props,"mode"),history&&!demo);
    obs_property_set_visible(obs_properties_get(props,"history"),!demo&&history&&obs_data_get_int(settings,"mode")==1);
    obs_property_set_visible(obs_properties_get(props,"encoding"),!demo&&provider=="virtualdj"&&obs_data_get_int(settings,"mode")==1);
    obs_property_set_visible(obs_properties_get(props,"receiver_profile"),remote&&!demo);
    obs_property_set_visible(obs_properties_get(props,"pair_receiver"),!demo);
    obs_property_set_visible(obs_properties_get(props,"title_minimum"),!normal&&obs_data_get_int(settings,"title_overflow")==3);
    obs_property_set_visible(obs_properties_get(props,"artist_minimum"),!normal&&obs_data_get_int(settings,"artist_overflow")==3);
    obs_property_set_visible(obs_properties_get(props,"normal_preset"),false);obs_property_set_visible(obs_properties_get(props,"pro_preset"),false);
    // One editor: only the controls selected for the simplified panel are shown.
    // Older scene/preset keys and callbacks are retained without exposing them.
    const char *shown[]={"pair_receiver","show_existing","artwork_enabled","artwork_fallback","twitch_profile_auto",
        "chat_enabled","chat_connect","chat_message","chat_delay",
        "provider","mode","preset_family","preset_style","pro_hue","pro_light_intensity","pro_light_pulse",
        "pro_panel_opacity","animation_speed","normal_transition","normal_duration","hide_seconds","title_overflow","artist_overflow","canvas_width","canvas_height",
        "text_title_font","text_artist_font","text_title_color","text_artist_color","text_offset_x","text_offset_y","title_align","artist_align",
        "demo_enabled","demo_previous","demo_next","status","nexus_credit"};
    for(auto property=obs_properties_first(props);property;obs_property_next(&property)){
        const auto key=obs_property_name(property);
        if(std::none_of(std::begin(shown),std::end(shown),[key](const char *value){return std::strcmp(key,value)==0;}))obs_property_set_visible(property,false);
    }
    for(const auto key:{"chat_connect","chat_message","chat_delay"})obs_property_set_visible(obs_properties_get(props,key),obs_data_get_bool(settings,"chat_enabled"));
    return true;
}
struct PropertiesContext {std::shared_ptr<State> state;long long geometry=0;};
long long geometryStyle(obs_data_t *settings){const auto pro=obs_data_get_int(settings,"pro_preset");return pro?pro:-(1+obs_data_get_int(settings,"normal_preset"));}
void changeStyleGeometry(obs_properties_t *props,obs_data_t *settings){
    const auto current=geometryStyle(settings);
    auto context=static_cast<PropertiesContext *>(obs_properties_get_param(props));if(!context)return;auto previousValue=&context->geometry;
    const auto previous=*previousValue;if(current==previous)return;
    // Selecting a style restores its authored geometry, as requested.
    // Appearance (font, colours, effects and animation) remains independent.
    const char *keys[]={"canvas_width","canvas_height","text_layout","text_offset_x","text_offset_y","text_margin","text_line_gap","text_top_distance","text_image_gap","text_cover_size","normal_gap","artwork_x","artwork_y","artwork_width","artwork_height","title_x","title_y","artist_x","artist_y","title_width","title_height","artist_width","artist_height","title_align","artist_align"};
    for(const auto key:keys)obs_data_unset_user_value(settings,key);
    *previousValue=current;
}
bool textOrientation(obs_properties_t *props,obs_property_t *property,obs_data_t *settings) {
    changeStyleGeometry(props,settings);
    if(property&&std::string(obs_property_name(property))=="pro_preset"&&obs_data_get_int(settings,"pro_preset")!=0)obs_data_set_int(settings,"normal_preset",0);
    const auto selection=obs_data_get_int(settings,"pro_preset");
    const auto direction=selection==3?2:selection==2?1:obs_data_get_int(settings,"text_layout");
    const bool pro=obs_data_get_int(settings,"pro_preset")==1||obs_data_get_int(settings,"pro_preset")==4||obs_data_get_int(settings,"pro_preset")==5||obs_data_get_int(settings,"pro_preset")==6||obs_data_get_int(settings,"pro_preset")==7||obs_data_get_int(settings,"pro_preset")==8||obs_data_get_int(settings,"pro_preset")==9||obs_data_get_int(settings,"pro_preset")==10||obs_data_get_int(settings,"pro_preset")==11||obs_data_get_int(settings,"pro_preset")==12||obs_data_get_int(settings,"pro_preset")==13||obs_data_get_int(settings,"pro_preset")==14||obs_data_get_int(settings,"pro_preset")==15||obs_data_get_int(settings,"pro_preset")==16;
    obs_data_set_default_int(settings,"canvas_width",pro?(selection==16?480:selection==15?384:selection==14?400:selection==13?416:selection==12?480:960):(direction==2?320:640));obs_data_set_default_int(settings,"canvas_height",pro?(selection>=12?720:obs_data_get_int(settings,"pro_preset")>=7?320:obs_data_get_int(settings,"pro_preset")>=5?280:367):(direction==2?360:120));
    return visibility(props,nullptr,settings);
}
bool classicSelection(obs_properties_t *props,obs_property_t *,obs_data_t *settings){
    if(obs_data_get_int(settings,"normal_preset")!=0)obs_data_set_int(settings,"pro_preset",0);
    return visibility(props,nullptr,settings);
}
int presetFamily(obs_data_t *settings){
    if(obs_data_has_user_value(settings,"preset_family"))return obs_data_get_int(settings,"preset_family")==1?1:0;
    return obs_data_get_int(settings,"normal_preset")!=0&&obs_data_get_int(settings,"pro_preset")==0?0:1;
}
bool populateStyle(obs_properties_t *props,obs_data_t *settings){
    const int family=presetFamily(settings);auto *style=obs_properties_get(props,"preset_style");obs_property_list_clear(style);
    obs_property_list_add_int(style,playing::ui::text("Seleccionar estilo"),0);
    if(family){
        obs_property_list_add_int(style,"Neon Club",1);obs_property_list_add_int(style,"Vinyl Classic",4);obs_property_list_add_int(style,"Minimal",5);obs_property_list_add_int(style,"Glassmorph",6);obs_property_list_add_int(style,"Urban",7);obs_property_list_add_int(style,"Waveform",8);obs_property_list_add_int(style,"HUD",9);obs_property_list_add_int(style,"Particles",10);obs_property_list_add_int(style,"Liquid",11);obs_property_list_add_int(style,"Vertical Retro",12);obs_property_list_add_int(style,"Vertical Glass",13);obs_property_list_add_int(style,"Vertical Vinyl",14);obs_property_list_add_int(style,"Vertical Urban",15);obs_property_list_add_int(style,"Vertical Liquid",16);
        obs_property_list_add_int(style,playing::ui::text("Solo texto horizontal"),2);obs_property_list_add_int(style,playing::ui::text("Solo texto vertical"),3);
    }else{
        for(const auto &preset:visual::normal::normalCatalogue())obs_property_list_add_int(style,preset.name.c_str(),preset.setting);
        // Old aliases remain loadable without lengthening the normal new-style list.
        for(const auto &alias:visual::normal::normalAliases())if(alias.setting==obs_data_get_int(settings,"normal_preset"))obs_property_list_add_int(style,alias.name.c_str(),alias.setting);
    }
    obs_data_set_default_int(settings,"preset_family",family);
    obs_data_set_int(settings,"preset_style",obs_data_get_int(settings,family?"pro_preset":"normal_preset"));
    return visibility(props,nullptr,settings);
}
bool categoryChanged(obs_properties_t *props,obs_property_t *,obs_data_t *settings){
    const int family=presetFamily(settings);
    if((family&&obs_data_get_int(settings,"normal_preset")!=0)||(!family&&obs_data_get_int(settings,"pro_preset")!=0)){
        obs_data_set_int(settings,"normal_preset",family?0:visual::normal::normalCatalogue().front().setting);
        obs_data_set_int(settings,"pro_preset",family?1:0);
    }
    populateStyle(props,settings);return textOrientation(props,nullptr,settings);
}
bool styleChanged(obs_properties_t *props,obs_property_t *,obs_data_t *settings){
    const int family=presetFamily(settings);const auto selected=obs_data_get_int(settings,"preset_style");
    obs_data_set_int(settings,"normal_preset",family?0:selected);obs_data_set_int(settings,"pro_preset",family?selected:0);
    return textOrientation(props,nullptr,settings);
}
bool openPresetManagerImpl(void *data,bool saveAs) {
    auto source=static_cast<Source *>(data);if(!source||!source->source||service.closing.load())return false;
    auto weak=std::shared_ptr<obs_weak_source_t>(obs_source_get_weak_source(source->source),obs_weak_source_release);
    char* rawPath=obs_module_config_path("presets-v1.json");if(!rawPath)return false;
    const auto path=QString::fromUtf8(rawPath);bfree(rawPath);
    std::lock_guard<std::mutex> gate(service.dispatchMutex);if(service.closing.load())return false;
    QMetaObject::invokeMethod(service.dispatcher,[weak,path,saveAs]{
        if(service.closing.load())return;
        try {
            auto acquire=[weak]{if(service.closing.load())throw std::runtime_error(playing::ui::text("OBS se está cerrando."));auto target=std::shared_ptr<obs_source_t>(obs_weak_source_get_source(weak.get()),obs_source_release);if(!target||obs_source_removed(target.get()))throw std::runtime_error(playing::ui::text("La fuente se ha cerrado o eliminado."));return target;};
            auto current=[acquire]{auto target=acquire();auto settings=std::shared_ptr<obs_data_t>(obs_source_get_settings(target.get()),obs_data_release);auto result=QJsonDocument::fromJson(QByteArray(obs_data_get_json(settings.get()))).object();result["normal_preset"]=qint64(obs_data_get_int(settings.get(),"normal_preset"));result["pro_preset"]=qint64(obs_data_get_int(settings.get(),"pro_preset"));return result;};
            auto apply=[acquire](const presets::Entry& entry){
                if(service.closing.load())throw std::runtime_error(playing::ui::text("OBS se está cerrando."));
                presets::validate(entry.appearance);auto target=acquire();auto settings=std::shared_ptr<obs_data_t>(obs_source_get_settings(target.get()),obs_data_release);
                for(const auto& key:presets::visualKeys()){const auto utf8=key.toUtf8();obs_data_unset_user_value(settings.get(),utf8.constData());}
                const auto json=QJsonDocument(entry.appearance).toJson(QJsonDocument::Compact);auto appearance=std::shared_ptr<obs_data_t>(obs_data_create_from_json(json.constData()),obs_data_release);obs_data_apply(settings.get(),appearance.get());
                const auto id=entry.id.toUtf8();obs_data_set_string(settings.get(),"preset_manager_id",id.constData());obs_source_update(target.get(),settings.get());obs_source_update_properties(target.get());
            };
            auto dialog=presets::showManager(path,current,apply,static_cast<QWidget *>(obs_frontend_get_main_window()));service.dialogs.push_back(dialog);
            if(saveAs)QTimer::singleShot(0,dialog,[dialog]{if(auto button=dialog->findChild<QPushButton*>("presetSaveAs"))button->click();});
        }catch(const std::exception& error){blog(LOG_WARNING,"[playing-next] Preset manager unavailable: %s",error.what());QMessageBox::warning(static_cast<QWidget *>(obs_frontend_get_main_window()),playing::ui::text("Gestor de presets"),QString::fromUtf8(error.what()));}
    },Qt::QueuedConnection);return false;
}
bool openPresetManager(obs_properties_t *,obs_property_t *,void *data){return openPresetManagerImpl(data,false);}
bool saveDesignAs(obs_properties_t *,obs_property_t *,void *data){return openPresetManagerImpl(data,true);}
bool pairReceiver(obs_properties_t *props,obs_property_t *,void *data) {
    auto stored=static_cast<PropertiesContext *>(obs_properties_get_param(props));if(!stored||!stored->state)return false;
    const auto state=stored->state;
    const auto source=static_cast<Source *>(data);
    auto weak=source&&source->source?std::shared_ptr<obs_weak_source_t>(obs_source_get_weak_source(source->source),obs_weak_source_release):std::shared_ptr<obs_weak_source_t>{};
    if(service.closing.load())return false;
    std::lock_guard<std::mutex> gate(service.dispatchMutex);
    if(service.closing.load())return false;
    QMetaObject::invokeMethod(service.dispatcher,[state,weak] {
        QString path;{std::lock_guard<std::mutex> lock(state->mutex);if(state->closed)return;path=state->profilePath;}
        try {
            auto changed=[weak,path] {
                bool linked=false;
                try{const auto saved=companion::ProtectedStore(path).load();linked=saved&&companion::decodeProfile(*saved).receiver.has_value();}catch(const std::exception &){}
                auto selected=weak?std::shared_ptr<obs_source_t>(obs_weak_source_get_source(weak.get()),obs_source_release):std::shared_ptr<obs_source_t>{};
                if(linked&&selected&&!obs_source_removed(selected.get())){
                    auto settings=obs_source_get_settings(selected.get());obs_data_set_string(settings,"provider","network");obs_data_set_int(settings,"mode",0);
                    obs_source_update(selected.get(),settings);obs_data_release(settings);obs_source_update_properties(selected.get());
                }
                const auto states=service.states;
                for(const auto &entryWeak:states)if(auto target=entryWeak.lock()) {
                    {std::lock_guard<std::mutex> lock(target->mutex);if(target->closed||target->request.providerId!="network"||target->profilePath!=path)continue;++target->revision;}
                    target->consumer->publication.publish({});command(target);
                }
            };
            const auto health=[state]{PlayingReceiverHealth result;
                if(service.closing)return result;
                {std::lock_guard<std::mutex> lock(state->mutex);if(state->closed)return result;if(!state->active)result.message=playing::ui::text("Activa esta fuente en la escena de OBS para comprobar la recepción.");}
                result.connected=service.host&&service.host->remoteConnected(state->attached.providerId);
                const auto frame=state->consumer->publication.read();result.track=frame&&frame->snapshot&&frame->snapshot->metadata.has_value();result.image=frame&&frame->artwork;
                if(result.connected&&!result.track&&result.message.isEmpty())result.message=playing::ui::text("La red funciona. Reproduce una canción en el PC DJ.");
                return result;
            };
            bool linked=false;try{const auto previous=companion::ProtectedStore(path).load();linked=previous&&companion::decodeProfile(*previous).receiver.has_value();}catch(const std::exception &){}
            if(linked)changed();
            service.dialogs.push_back(showPlayingReceiverLink(path,changed,static_cast<QWidget *>(obs_frontend_get_main_window()),health));
        }catch(const std::exception &error){blog(LOG_WARNING,"[playing-next] Pairing unavailable: %s",error.what());}
    },Qt::QueuedConnection);return false;
}
bool reloadArtwork(obs_properties_t *props,obs_property_t *,void *) {
    const auto stored=static_cast<PropertiesContext *>(obs_properties_get_param(props));if(!stored||!stored->state)return false;
    auto state=stored->state;
    {std::lock_guard<std::mutex> lock(state->mutex);if(state->closed)return false;
     if(state->automaticProfileEnabled&&!state->manualProfile){state->profileRequested=false;state->profileFailures=0;++state->profileGeneration;state->automaticProfile.reset();state->textLayout.streamerProfile.reset();++state->revision;}
     else if(state->textLayout.streamerProfile){state->textLayout.streamerProfile->version="reload-"+std::to_string(++imageVersions);++state->revision;}}
    QMetaObject::invokeMethod(service.dispatcher,[]{if(service.profiles)service.profiles->invalidate();},Qt::QueuedConnection);
    command(state);return false;
}
bool imageDiagnostics(obs_properties_t *props,obs_property_t *,void *) {
    const auto stored=static_cast<PropertiesContext *>(obs_properties_get_param(props));if(!stored||!stored->state)return false;
    const auto state=stored->state;
    QMetaObject::invokeMethod(service.dispatcher,[state]{
        if(service.closing)return;
        QString text;
        {std::lock_guard<std::mutex> lock(state->mutex);if(state->closed)return;
         text=playing::ui::string("Mostrar imagen: %1\nAlternativa del perfil: %2\nPerfil automático de Twitch: %3\nImagen manual configurada: %4\n")
             .arg(state->textLayout.artworkEnabled?playing::ui::text("Sí"):playing::ui::text("No"),state->textLayout.profileFallback?playing::ui::text("Sí"):playing::ui::text("No"),state->automaticProfileEnabled?playing::ui::text("Sí"):playing::ui::text("No"),state->manualProfile?playing::ui::text("Sí"):playing::ui::text("No"));
         text+=QStringLiteral("Twitch: ")+(state->profileStatus.isEmpty()?playing::ui::string("Sin consulta realizada."):state->profileStatus)+"\n";}
        const auto frame=state->consumer->publication.read();
        QString cover=playing::ui::text("Sin canción activa");
        if(frame&&frame->snapshot&&frame->snapshot->track){switch(frame->snapshot->track->artwork.status){
            case core::ArtworkStatus::Ready:cover=playing::ui::text("Portada recibida");break;
            case core::ArtworkStatus::Missing:cover=playing::ui::text("El lector DJ no proporciona portada");break;
            case core::ArtworkStatus::Failed:cover=playing::ui::text("Error de portada en el lector DJ");break;
            default:cover=playing::ui::text("Portada todavía no disponible");break;}}
        text+=playing::ui::string("Portada: ")+cover+playing::ui::string("\nImagen preparada para dibujar: ")+(frame&&frame->artwork?playing::ui::text("Sí"):playing::ui::text("No"));
        QMessageBox::information(static_cast<QWidget *>(obs_frontend_get_main_window()),playing::ui::string("Diagnóstico de imágenes"),text);
    },Qt::QueuedConnection);return false;
}
bool manualProfileImage(obs_properties_t *,obs_property_t *property,void *data) {
    auto source=static_cast<Source *>(data);if(!source||!source->source||service.closing.load())return false;
    const auto action=std::string(obs_property_name(property));
    auto weak=std::shared_ptr<obs_weak_source_t>(obs_source_get_weak_source(source->source),obs_weak_source_release);
    QMetaObject::invokeMethod(service.dispatcher,[weak,action]{
        if(service.closing.load())return;
        QString chosen;bool accepted=action=="clear_streamer_image";
        if(action=="choose_streamer_image"){
            chosen=QFileDialog::getOpenFileName(static_cast<QWidget *>(obs_frontend_get_main_window()),playing::ui::string("Seleccionar imagen manual"),QString{},playing::ui::string("Imagenes (*.png *.jpg *.jpeg *.webp *.svg)"));accepted=!chosen.isEmpty();
        }else if(action=="manual_profile_url"){
            chosen=QInputDialog::getText(static_cast<QWidget *>(obs_frontend_get_main_window()),playing::ui::string("Imagen manual por URL"),playing::ui::string("URL HTTPS (opcional)"),QLineEdit::Normal,QString{},&accepted).trimmed();
            if(accepted&&!chosen.isEmpty()&&!chosen.startsWith("https://")){QMessageBox::warning(static_cast<QWidget *>(obs_frontend_get_main_window()),playing::ui::string("Imagen manual"),playing::ui::string("La URL debe comenzar con https://"));return;}
        }
        if(!accepted||service.closing.load())return;
        auto target=std::shared_ptr<obs_source_t>(obs_weak_source_get_source(weak.get()),obs_source_release);
        if(!target||obs_source_removed(target.get()))return;
        auto settings=obs_source_get_settings(target.get());const auto utf8=chosen.toUtf8();
        obs_data_set_string(settings,"streamer_image",utf8.constData());obs_data_set_string(settings,"streamer_image_version","");
        obs_source_update(target.get(),settings);obs_data_release(settings);obs_source_update_properties(target.get());
    },Qt::QueuedConnection);return false;
}
bool previewTrack(obs_properties_t *,obs_property_t *property,void *data) {
    auto source=static_cast<Source *>(data);if(!source||!source->source)return false;
    auto settings=obs_source_get_settings(source->source);
    const int direction=std::string(obs_property_name(property))=="demo_previous"?-1:1;
    const int index=(int(obs_data_get_int(settings,"demo_index"))+direction+3)%3;
    const char *titles[]={"Night Drive (Original Mix)","Summer Lights (Extended Mix)","After Midnight"};
    const char *artists[]={"Neon Avenue","Luna Coast","City Echoes"};
    obs_data_set_int(settings,"demo_index",index);obs_data_set_bool(settings,"demo_enabled",true);
    obs_data_set_string(settings,"demo_title",titles[index]);obs_data_set_string(settings,"demo_artist",artists[index]);
    obs_source_update(source->source,settings);obs_data_release(settings);return true;
}
bool connectChat(obs_properties_t *,obs_property_t *,void *){
    if(service.closing)return false;
    QMetaObject::invokeMethod(service.dispatcher,[]{if(service.chat&&!service.closing)service.dialogs.push_back(service.chat->dialog(static_cast<QWidget *>(obs_frontend_get_main_window())));},Qt::QueuedConnection);return false;
}
obs_properties_t *properties(void *data) {
    auto stored=new PropertiesContext{data?static_cast<Source *>(data)->state:std::shared_ptr<State>{},0};
    auto props=obs_properties_create_param(stored,[](void *value){delete static_cast<PropertiesContext *>(value);});
    obs_properties_add_button2(props,"pair_receiver",playing::ui::text("Conectar Companion"),pairReceiver,data);
    obs_properties_add_bool(props,"show_existing",playing::ui::text("Mostrar el tema encontrado al activar la fuente"));
    auto cover=obs_properties_add_bool(props,"artwork_enabled",playing::ui::text("Mostrar portada / imagen de perfil"));obs_property_set_modified_callback(cover,visibility);
    obs_properties_add_bool(props,"artwork_fallback",playing::ui::text("Usar perfil del streamer si no hay portada"));
    obs_properties_add_bool(props,"twitch_profile_auto",playing::ui::text("Obtener imagen del canal Twitch conectado en OBS"));
    auto chatToggle=obs_properties_add_bool(props,"chat_enabled",playing::ui::text("Anunciar canciones en el chat de Twitch"));obs_property_set_modified_callback(chatToggle,visibility);
    obs_properties_add_button2(props,"chat_connect",playing::ui::text("Conectar chat de Twitch…"),connectChat,data);
    auto chatMessage=obs_properties_add_text(props,"chat_message",playing::ui::text("Mensaje del chat"),OBS_TEXT_DEFAULT);
    obs_property_set_long_description(chatMessage,playing::ui::text("Incluye {title} y {artist}. Solo canciones reales durante la transmisión; los mensajes repetidos se omiten."));
    obs_properties_add_int_slider(props,"chat_delay",playing::ui::text("Espera antes del anuncio (s)"),1,30,1);
    auto provider=obs_properties_add_list(props,"provider",playing::ui::text("Programa DJ"),OBS_COMBO_TYPE_LIST,OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(provider,"VirtualDJ","virtualdj");obs_property_list_add_string(provider,playing::ui::text("Serato DJ Pro (Automatic)"),"serato");
    obs_property_list_add_string(provider,"Traktor Pro 3 / 4","traktor");
    if(!service.databaseLibrary.empty())obs_property_list_add_string(provider,"rekordbox","rekordbox");
    obs_property_list_add_string(provider,playing::ui::text("Companion — PC DJ"),"network");
    auto mode=obs_properties_add_list(props,"mode",playing::ui::text("Lectura"),OBS_COMBO_TYPE_LIST,OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(mode,playing::ui::text("Automatic"),0);obs_property_list_add_int(mode,playing::ui::text("History"),1);
    auto category=obs_properties_add_list(props,"preset_family",playing::ui::text("Categoria"),OBS_COMBO_TYPE_LIST,OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(category,playing::ui::text("Clasicos"),0);obs_property_list_add_int(category,"PRO",1);obs_property_set_modified_callback(category,categoryChanged);
    auto selectedStyle=obs_properties_add_list(props,"preset_style",playing::ui::text("Estilo"),OBS_COMBO_TYPE_LIST,OBS_COMBO_FORMAT_INT);obs_property_set_modified_callback(selectedStyle,styleChanged);
    obs_properties_add_button2(props,"preset_manager",playing::ui::text("Gestor de presets / favoritos"),openPresetManager,data);
    obs_properties_add_button2(props,"preset_save_as",playing::ui::text("Guardar diseno como..."),saveDesignAs,data);
    obs_properties_add_text(props,"editor_hint",playing::ui::text("Layout"),OBS_TEXT_INFO);
    auto normal=obs_properties_add_list(props,"normal_preset",playing::ui::text("Clasicos"),OBS_COMBO_TYPE_LIST,OBS_COMBO_FORMAT_INT);
    obs_property_set_modified_callback(normal,classicSelection);
    auto pro=obs_properties_add_list(props,"pro_preset","PRO",OBS_COMBO_TYPE_LIST,OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(pro,playing::ui::text("Desactivado"),0);obs_property_list_add_int(pro,"Neon Club",1);obs_property_list_add_int(pro,playing::ui::text("Solo texto horizontal"),2);obs_property_list_add_int(pro,playing::ui::text("Solo texto vertical"),3);obs_property_list_add_int(pro,"Vinyl Classic",4);obs_property_list_add_int(pro,"Minimal",5);obs_property_list_add_int(pro,"Glassmorph",6);obs_property_list_add_int(pro,"Urban",7);obs_property_list_add_int(pro,"Waveform",8);obs_property_list_add_int(pro,"HUD",9);obs_property_list_add_int(pro,"Particles",10);obs_property_list_add_int(pro,"Liquid",11);obs_property_list_add_int(pro,"Vertical Retro",12);obs_property_list_add_int(pro,"Vertical Glass",13);obs_property_list_add_int(pro,"Vertical Vinyl",14);obs_property_list_add_int(pro,"Vertical Urban",15);obs_property_list_add_int(pro,"Vertical Liquid",16);obs_property_set_modified_callback(pro,textOrientation);
    obs_properties_add_int_slider(props,"pro_hue",playing::ui::text("Tono general (0/360: original)"),0,360,1);
    obs_properties_add_int_slider(props,"pro_light_intensity",playing::ui::text("Intensidad de luz (%)"),0,100,1);
    obs_properties_add_bool(props,"pro_light_pulse",playing::ui::text("Animar pulso de luz (opcional)"));obs_properties_add_int_slider(props,"pro_panel_opacity",playing::ui::text("Opacidad del fondo del titulo (%)"),0,100,1);
    obs_properties_add_int_slider(props,"animation_speed",playing::ui::text("Velocidad de animacion (%)"),25,300,5);
    obs_property_list_add_int(normal,playing::ui::text("Desactivado / texto manual"),0);
    for(std::size_t i=0;i<visual::normal::normalCatalogue().size();++i)obs_property_list_add_int(normal,visual::normal::normalCatalogue()[i].name.c_str(),static_cast<long long>(visual::normal::normalCatalogue()[i].setting));
    for(std::size_t i=0;i<visual::normal::normalAliases().size();++i)obs_property_list_add_int(normal,visual::normal::normalAliases()[i].name.c_str(),static_cast<long long>(visual::normal::normalAliases()[i].setting));
    auto transition=obs_properties_add_list(props,"normal_transition",playing::ui::text("Transicion del tema"),OBS_COMBO_TYPE_LIST,OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(transition,playing::ui::text("Fade"),0);obs_property_list_add_int(transition,playing::ui::text("Slide"),1);obs_property_list_add_int(transition,"Zoom",2);
    obs_properties_add_int(props,"normal_duration",playing::ui::text("Duracion de transicion (ms)"),200,2000,50);
    obs_properties_add_bool(props,"transition_whole",playing::ui::text("Transicion de toda la tarjeta"));
    obs_properties_add_int(props,"normal_gap",playing::ui::text("Separacion de lineas (px)"),0,80,1);
    obs_properties_add_int(props,"normal_speed",playing::ui::text("Velocidad del texto (px/s)"),10,150,1);
    obs_properties_add_int(props,"normal_pause",playing::ui::text("Pausa del texto (s)"),0,10,1);
    obs_properties_add_path(props,"history",playing::ui::text("Historial / biblioteca (vacio: detectar)"),OBS_PATH_FILE,playing::ui::text("All (*.*)"),nullptr);
    auto encoding=obs_properties_add_list(props,"encoding",playing::ui::text("Codificacion de historial VirtualDJ"),OBS_COMBO_TYPE_LIST,OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(encoding,playing::ui::text("Automatic"),0);obs_property_list_add_int(encoding,"ANSI",1);obs_property_list_add_int(encoding,"UTF-16LE",2);
    obs_properties_add_path(props,"receiver_profile",playing::ui::text("Perfil protegido de vinculacion"),OBS_PATH_FILE,playing::ui::text("Profile (*.pcs)"),nullptr);
    obs_properties_add_text(props,"twitch_channel",playing::ui::text("Canal Twitch (opcional; vacio usa tu cuenta OBS)"),OBS_TEXT_DEFAULT);
    obs_properties_add_button2(props,"choose_streamer_image",playing::ui::text("Seleccionar imagen manual..."),manualProfileImage,data);
    obs_properties_add_button2(props,"manual_profile_url",playing::ui::text("Introducir URL HTTPS (opcional)"),manualProfileImage,data);
    obs_properties_add_button2(props,"clear_streamer_image",playing::ui::text("Quitar imagen manual"),manualProfileImage,data);
    obs_properties_add_button(props,"reload_artwork",playing::ui::text("Recargar imagen de perfil"),reloadArtwork);
    obs_properties_add_button(props,"image_diagnostics",playing::ui::text("Diagnóstico de imágenes"),imageDiagnostics);
    for(const auto &field:{std::pair<const char *,const char *>{"artwork_x",playing::ui::text("Imagen: posicion X")},{"artwork_y",playing::ui::text("Imagen: posicion Y")}})obs_properties_add_int(props,field.first,field.second,0,2047,1);
    for(const auto &field:{std::pair<const char *,const char *>{"artwork_width",playing::ui::text("Imagen: ancho")},{"artwork_height",playing::ui::text("Imagen: alto")}})obs_properties_add_int(props,field.first,field.second,1,2048,1);
    obs_properties_add_int(props,"hide_seconds",playing::ui::text("Ocultar tema tras segundos (0: sin limite)"),0,3600,1);
    for(const auto &field: {std::pair<const char *,const char *>{"title_overflow",playing::ui::text("Titulo largo")},{"artist_overflow",playing::ui::text("Artista largo")}}){
        auto property=obs_properties_add_list(props,field.first,field.second,OBS_COMBO_TYPE_LIST,OBS_COMBO_FORMAT_INT);
        obs_property_list_add_int(property,playing::ui::text("Recortar"),0);obs_property_list_add_int(property,playing::ui::text("Puntos suspensivos"),1);
        obs_property_list_add_int(property,playing::ui::text("Varias lineas"),2);obs_property_list_add_int(property,playing::ui::text("Ajustar tamano"),3);
        obs_property_list_add_int(property,playing::ui::text("Ping-pong (ida y vuelta)"),4);
        obs_property_set_modified_callback(property,visibility);
    }
    obs_properties_add_int(props,"title_minimum",playing::ui::text("Tamano minimo del titulo (px)"),8,30,1);
    obs_properties_add_int(props,"artist_minimum",playing::ui::text("Tamano minimo del artista (px)"),8,22,1);
    obs_properties_add_int(props,"canvas_width",playing::ui::text("Ancho de la fuente (px)"),64,2048,1);
    obs_properties_add_int(props,"canvas_height",playing::ui::text("Alto de la fuente (px)"),64,2048,1);
    auto textLayout=obs_properties_add_list(props,"text_layout",playing::ui::text("Diseno de texto"),OBS_COMBO_TYPE_LIST,OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(textLayout,playing::ui::text("Manual (BASIC actual)"),0);obs_property_list_add_int(textLayout,playing::ui::text("Solo texto — horizontal"),1);obs_property_list_add_int(textLayout,playing::ui::text("Solo texto — vertical"),2);obs_property_set_modified_callback(textLayout,textOrientation);
    obs_properties_add_font(props,"text_title_font",playing::ui::text("Tipografia y tamano del titulo"));obs_properties_add_font(props,"text_artist_font",playing::ui::text("Tipografia y tamano del artista"));
    obs_properties_add_color_alpha(props,"text_title_color",playing::ui::text("Color del titulo"));obs_properties_add_color_alpha(props,"text_artist_color",playing::ui::text("Color del artista"));
    for(const auto &row:{std::pair<const char*,const char*>{"title_",playing::ui::text("Titulo")},{"artist_",playing::ui::text("Artista")}}){const std::string prefix=row.first,label=row.second;obs_properties_add_int_slider(props,(prefix+"outline_width").c_str(),(label+playing::ui::text(": grosor de contorno (px)")).c_str(),0,8,1);obs_properties_add_color_alpha(props,(prefix+"outline_color").c_str(),(label+playing::ui::text(": color de contorno")).c_str());obs_properties_add_int_slider(props,(prefix+"shadow_offset").c_str(),(label+playing::ui::text(": distancia de sombra (px)")).c_str(),0,8,1);obs_properties_add_color_alpha(props,(prefix+"shadow_color").c_str(),(label+playing::ui::text(": color y opacidad de sombra")).c_str());}
    obs_properties_add_int_slider(props,"text_offset_x",playing::ui::text("Desplazamiento horizontal del texto (px)"),-1024,1024,1);
    obs_properties_add_int_slider(props,"text_offset_y",playing::ui::text("Desplazamiento vertical del texto (px)"),-1024,1024,1);
    obs_properties_add_int(props,"text_margin",playing::ui::text("Margen exterior (px)"),0,128,1);obs_properties_add_int(props,"text_line_gap",playing::ui::text("Separacion titulo / artista (px)"),0,128,1);
    obs_properties_add_int_slider(props,"text_top_distance",playing::ui::text("Distancia del texto desde arriba (px)"),0,2048,1);
    obs_properties_add_int(props,"text_image_gap",playing::ui::text("Distancia portada / texto (px)"),0,128,1);obs_properties_add_int(props,"text_cover_size",playing::ui::text("Tamano de portada (px)"),16,512,1);
    for(const auto &field: {std::pair<const char *,const char *>{"title_x",playing::ui::text("Titulo: posicion X")},{"title_y",playing::ui::text("Titulo: posicion Y")},{"artist_x",playing::ui::text("Artista: posicion X")},{"artist_y",playing::ui::text("Artista: posicion Y")}})
        obs_properties_add_int(props,field.first,field.second,0,2047,1);
    for(const auto &field: {std::pair<const char *,const char *>{"title_width",playing::ui::text("Titulo: ancho")},{"title_height",playing::ui::text("Titulo: alto")},{"artist_width",playing::ui::text("Artista: ancho")},{"artist_height",playing::ui::text("Artista: alto")}})
        obs_properties_add_int(props,field.first,field.second,1,2048,1);
    for(const auto &field: {std::pair<const char *,const char *>{"title_align",playing::ui::text("Alineacion del titulo")},{"artist_align",playing::ui::text("Alineacion del artista")}}){
        auto property=obs_properties_add_list(props,field.first,field.second,OBS_COMBO_TYPE_LIST,OBS_COMBO_FORMAT_INT);
        obs_property_list_add_int(property,playing::ui::text("Izquierda"),0);obs_property_list_add_int(property,playing::ui::text("Centro"),1);obs_property_list_add_int(property,playing::ui::text("Derecha"),2);
    }
    if(stored->state){std::lock_guard<std::mutex> lock(stored->state->mutex);if(!stored->state->error.empty())obs_properties_add_text(props,"status",playing::ui::text("Configuracion no disponible: revisa programa, ruta o vinculacion."),OBS_TEXT_INFO);}
    auto demo=obs_properties_add_bool(props,"demo_enabled",playing::ui::text("Tema de prueba (ficticio)"));obs_property_set_modified_callback(demo,visibility);
    obs_properties_add_button2(props,"demo_previous",playing::ui::text("Probar tema anterior"),previewTrack,data);
    obs_properties_add_button2(props,"demo_next",playing::ui::text("Probar tema siguiente"),previewTrack,data);
    obs_properties_add_path(props,"demo_image",playing::ui::text("Portada de prueba (opcional)"),OBS_PATH_FILE,playing::ui::text("Images (*.png *.jpg *.jpeg *.webp *.svg)"),nullptr);
    obs_properties_add_text(props,"nexus_credit","NEXUS OSB Plugins by DjEmiMas",OBS_TEXT_INFO);
    obs_property_set_modified_callback(provider,visibility);obs_property_set_modified_callback(mode,visibility);
    for(const auto key:{"text_line_gap","text_title_font","text_artist_font"})obs_property_set_modified_callback(obs_properties_get(props,key),visibility);
    auto initial=data?obs_source_get_settings(static_cast<Source *>(data)->source):obs_data_create();
    stored->geometry=geometryStyle(initial);
    populateStyle(props,initial);obs_data_release(initial);
    return props;
}
void shutdownGui(void *) {
    {std::lock_guard<std::mutex> gate(service.dispatchMutex);
     service.closing=true;
     delete service.dispatcher;service.dispatcher=nullptr;service.profiles=nullptr;service.chat=nullptr;} // Cancels pending context-bound commands.
    for(auto &dialog:service.dialogs)delete dialog.data();service.dialogs.clear();
    service.host.reset();service.states.clear();clearGpu();
#ifdef PLAYING_SOURCE_TEST_HOOKS
    service.fixture.reset();
#endif
    blog(LOG_INFO,"[playing-next] GUI sessions, pending commands and graphics closed");
}
void frontend(enum obs_frontend_event event,void *) {
    if(event==OBS_FRONTEND_EVENT_EXIT) {
        if(QThread::currentThread()==qApp->thread())shutdownGui(nullptr);
        else obs_queue_task(OBS_TASK_UI,shutdownGui,nullptr,true);
    }
    else if(event==OBS_FRONTEND_EVENT_STREAMING_STARTED||event==OBS_FRONTEND_EVENT_STREAMING_STOPPING||event==OBS_FRONTEND_EVENT_STREAMING_STOPPED||event==OBS_FRONTEND_EVENT_PROFILE_CHANGED||event==OBS_FRONTEND_EVENT_FINISHED_LOADING){
        if(event==OBS_FRONTEND_EVENT_STREAMING_STARTED)service.streamStopping=false;
        if(event==OBS_FRONTEND_EVENT_STREAMING_STOPPING||event==OBS_FRONTEND_EVENT_STREAMING_STOPPED)service.streamStopping=true;
        std::lock_guard<std::mutex> gate(service.dispatchMutex);if(service.dispatcher&&!service.closing)QMetaObject::invokeMethod(service.dispatcher,[event]{
            if(event==OBS_FRONTEND_EVENT_STREAMING_STOPPING||event==OBS_FRONTEND_EVENT_STREAMING_STOPPED)service.chat->broadcast(false,{});else syncChatBroadcast();
        },Qt::QueuedConnection);
    }
}
}
bool obs_module_load(void) {
    if(!qobject_cast<QGuiApplication *>(QCoreApplication::instance()))return false;
    try{visual::normal::normalCatalogue();visual::normal::normalAliases();visual::animation::profiles();}
    catch(const std::exception &error){blog(LOG_ERROR,"[playing-next] Preset catalogue rejected: %s",error.what());return false;}
    service.dispatcher=new QObject;service.dispatcher->moveToThread(qApp->thread());service.profiles=new obs::TwitchProfiles(service.dispatcher);
    char *chatConfig=obs_module_config_path("twitch-chat.pcs");
    service.chat=new obs::ChatAnnouncer(QString::fromUtf8(chatConfig?chatConfig:""),service.dispatcher);bfree(chatConfig);
    service.tlsPluginRoot=QFileInfo(QString::fromUtf8(obs_get_module_binary_path(obs_current_module()))).dir().filePath("playing-qt-plugins");
    QFile backend(QDir(service.tlsPluginRoot).filePath("tls/qschannelbackend.dll"));
    if(backend.open(QIODevice::ReadOnly)&&QCryptographicHash::hash(backend.readAll(),QCryptographicHash::Sha256).toHex()=="bff4f5b7c44c42b902d202dd9aa61f6cbf7daeb353a0f400f933a056688987da"&&!QCoreApplication::libraryPaths().contains(service.tlsPluginRoot)) {
        QCoreApplication::addLibraryPath(service.tlsPluginRoot);service.addedTlsPath=true;
    }
    const auto dbPath=QFileInfo(QString::fromUtf8(obs_get_module_binary_path(obs_current_module()))).dir().filePath("obs-playing-db.dll");
    QFile database(dbPath);
    // Normal OBS discovery can supply a relative module data directory.
    // The visual asset host requires an absolute root, unlike explicit probe loads.
    service.assetRoot=QDir(QString::fromUtf8(obs_get_module_data_path(obs_current_module()))).absoluteFilePath("assets");
    if(database.open(QIODevice::ReadOnly)&&QCryptographicHash::hash(database.readAll(),QCryptographicHash::Sha256).toHex()=="5030decc6d914539e3b9b7e28aa4f6de1e7161dac6fd4b21eb02e6754d9b175e")service.databaseLibrary=QFileInfo(database).absoluteFilePath().toUtf8().toStdString();
    obs_frontend_add_event_callback(frontend,nullptr);
    obs_source_info info{};info.id="obs_playing_next_development";info.type=OBS_SOURCE_TYPE_INPUT;
    info.output_flags=OBS_SOURCE_VIDEO|OBS_SOURCE_CUSTOM_DRAW;
    info.get_name=name;info.create=create;info.destroy=destroy;info.get_width=width;info.get_height=height;
    info.video_render=render;info.update=update;info.activate=activate;info.deactivate=deactivate;
    info.get_defaults=defaults;info.get_properties=properties;obs_register_source(&info);
    blog(LOG_INFO,"[playing-next] Native DJ source registered (10-C)");return true;
}
void obs_module_unload(void) {
    obs_frontend_remove_event_callback(frontend,nullptr);
    // Normal EXIT closes resources before libobs graphics dies.
    if(service.dispatcher)obs_queue_task(OBS_TASK_UI,shutdownGui,nullptr,true);
    if(service.addedTlsPath)QCoreApplication::removeLibraryPath(service.tlsPluginRoot);
    blog(LOG_INFO,"[playing-next] Module unloaded");
}
#ifdef PLAYING_SOURCE_TEST_HOOKS
namespace {std::mutex evidenceMutex;QByteArray evidence;QByteArray captureHash;int capturePixels=0,captures=0;
void evidenceGui() {
    QJsonObject value;value["sessions"]=int(service.host?service.host->sessions():0);
    value["managerVisible"]=0;value["managerItems"]=0;value["managerCategory"]=0;
    for(const auto& dialog:service.dialogs)if(dialog&&dialog->findChild<QListWidget*>("presetList")){value["managerVisible"]=dialog->isVisible()?1:0;value["managerItems"]=dialog->findChild<QListWidget*>("presetList")->count();value["managerCategory"]=dialog->findChild<QComboBox*>("presetCategory")->currentIndex();}
    value["consumers"]=int(service.host?service.host->consumers():0);value["timer"]=service.host&&service.host->scheduled();
    value["animationTimer"]=service.host&&service.host->animationScheduled();
    value["uploads"]=qint64(service.uploads.load());value["draws"]=qint64(service.draws.load());
    value["renders"]=qint64(service.host?service.host->visualStats().renders:0);
    value["refreshes"]=qint64(service.host?service.host->refreshes():0);
    value["pollingMs"]=service.host?service.host->pollingIntervalMs():0;value["animationMs"]=service.host?service.host->animationIntervalMs():0;
    if(service.host){const auto stats=service.host->schedulingStats();value["animationPasses"]=qint64(stats.animationPasses);value["animationVisits"]=qint64(stats.animationVisits);value["staticSkips"]=qint64(stats.staticSkips);value["dynamicCopies"]=qint64(stats.dynamicCopies);}
    int frames=0,artworks=0;
    for(const auto &weak:service.states)if(const auto state=weak.lock())if(const auto frame=state->consumer->publication.read()) {
        ++frames;if(frame->artwork)++artworks;value["title"]=(frame->snapshot&&frame->snapshot->track)?QString::fromUtf8(frame->snapshot->track->title.c_str()):QString{};
        value["proArtReady"]=frame->proArt?1:0;
        value["titleY"]=frame->scrollingViewport[0].y();value["artistY"]=frame->scrollingViewport[1].y();
        value["titleX"]=frame->scrollingViewport[0].x();value["titleWidth"]=frame->scrollingViewport[0].width();value["titleHeight"]=frame->scrollingViewport[0].height();value["artistX"]=frame->scrollingViewport[1].x();value["artistWidth"]=frame->scrollingViewport[1].width();value["artistHeight"]=frame->scrollingViewport[1].height();
        if(frame->artwork){value["artworkWidth"]=frame->artwork->image.width();value["artworkHeight"]=frame->artwork->image.height();}
        if(frame->proArt&&frame->proArt->record)value["recordBottom"]=frame->proArt->recordPosition.y()+frame->proArt->record->image.height();
        {std::lock_guard<std::mutex> lock(state->mutex);value["appearanceRetained"]=state->retainAppearance;value["profileFailures"]=int(state->profileFailures);value["profileGeneration"]=qint64(state->profileGeneration);}
        if(!frame->snapshot||!frame->snapshot->track)continue; // Idle frame has no track. Test diagnostics only.
        value["artist"]=QString::fromUtf8(frame->snapshot->track->artist.c_str());
        value["trackRevision"]=static_cast<qint64>(frame->snapshot->trackRevision);
        value["demoTrack"]=frame->snapshot->track->origin&&frame->snapshot->track->origin->provider=="demo";
        value["transitioning"]=frame->normalSample.transitioning;
        value["transitionAlpha"]=int(frame->normalSample.transition.alpha*1000);
        value["incoming"]=frame->normalSample.transition.incoming;
        value["wholeTransition"]=frame->wholeTransition;
    }
    QJsonArray observed;
    for(const auto &weak:service.states)if(const auto state=weak.lock())if(state->profileTrace.load(std::memory_order_relaxed)){
        std::lock_guard<std::mutex> lock(state->mutex);
        for(const auto &event:state->profileEvents){QJsonObject sample;sample["title"]=QString::fromUtf8(event.title.c_str());sample["renderNs"]=QString::number(event.renderNs);sample["videoNs"]=QString::number(event.videoNs);observed.append(sample);}
    }
    if(!observed.isEmpty())value["profileEvents"]=observed;
    value["artworks"]=artworks;value["frames"]=frames;value["captureHash"]=QString::fromLatin1(captureHash);value["capturePixels"]=capturePixels;
    value["captures"]=captures;
    value["remoteReady"]=service.fixture?1:0;
    value["remoteClients"]=int(service.fixture?service.fixture->sender.clientCount():0);
    value["sslReady"]=QSslSocket::supportsSsl()?1:0;
    std::lock_guard<std::mutex> lock(evidenceMutex);evidence=QJsonDocument(value).toJson(QJsonDocument::Compact);
}
}
extern "C" __declspec(dllexport) void playing_source_test_query() {QMetaObject::invokeMethod(service.dispatcher,[]{evidenceGui();},Qt::QueuedConnection);}
extern "C" __declspec(dllexport) void playing_source_test_receiver_command(int action) {
    QMetaObject::invokeMethod(service.dispatcher,[action]{
        for(const auto &dialog:service.dialogs)if(dialog&&dialog->objectName()=="receiverAssistant"){
            if(action==1)dialog->findChild<QPushButton*>("receiverForget")->click();
            if(action==2)dialog->close();
            if(action==3){const auto folder=QFileInfo(QString::fromUtf8(obs_get_module_binary_path(obs_current_module()))).dir();dialog->grab().save(folder.filePath("receiver-assistant.png"));}
            break;
        }
    },Qt::QueuedConnection);
}
extern "C" __declspec(dllexport) void playing_source_test_profile_trace(obs_source_t *source,bool enabled){
    if(!source||std::strcmp(obs_source_get_id(source),"obs_playing_next_development")!=0)return;
    const auto data=static_cast<Source *>(obs_obj_get_data(source));if(!data)return;
    const auto state=data->state;
    state->profileTrace.store(false,std::memory_order_relaxed);
    {std::lock_guard<std::mutex> lock(state->mutex);state->profileEvents.clear();state->profileEvents.reserve(128);}
    state->profileTrace.store(enabled,std::memory_order_relaxed);
}
extern "C" __declspec(dllexport) void playing_source_test_scene_create(obs_data_t* settings) {
    obs_data_addref(settings);auto retained=std::shared_ptr<obs_data_t>(settings,obs_data_release);
    QMetaObject::invokeMethod(service.dispatcher,[retained]{
        auto source=obs_source_create("obs_playing_next_development","Preset manager fixture",retained.get(),nullptr);
        auto scene=obs_scene_create("Preset manager scene fixture");
        if(source&&scene){obs_scene_add(scene,source);obs_frontend_set_current_scene(obs_scene_get_source(scene));}
        if(source)obs_source_release(source);if(scene)obs_scene_release(scene);
    },Qt::QueuedConnection);
}
extern "C" __declspec(dllexport) void playing_source_test_scene_remove() {
    QMetaObject::invokeMethod(service.dispatcher,[]{
        auto removedSource=obs_get_source_by_name("Preset manager fixture");
        auto sceneSource=obs_get_source_by_name("Preset manager scene fixture");if(!sceneSource){if(removedSource)obs_source_release(removedSource);return;}
        obs_scene_enum_items(obs_scene_from_source(sceneSource),[](obs_scene_t*,obs_sceneitem_t* item,void*){
            if(std::strcmp(obs_source_get_name(obs_sceneitem_get_source(item)),"Preset manager fixture")==0)obs_sceneitem_remove(item);return true;
        },nullptr);obs_source_release(sceneSource);if(removedSource){obs_source_remove(removedSource);obs_source_release(removedSource);}
    },Qt::QueuedConnection);
}
extern "C" __declspec(dllexport) void playing_source_test_manager_command(int action,int index) {
    QMetaObject::invokeMethod(service.dispatcher,[action,index]{for(const auto& dialog:service.dialogs)if(dialog&&dialog->findChild<QListWidget*>("presetList")){
        if(action==1)dialog->findChild<QComboBox*>("presetCategory")->setCurrentIndex(index);
        if(action==2){dialog->findChild<QListWidget*>("presetList")->setCurrentRow(index);dialog->findChild<QPushButton*>("presetApply")->click();}
        if(action==3)dialog->findChild<QPushButton*>("presetFavorite")->click();
        if(action==5){QTimer::singleShot(0,dialog,[]{if(auto input=qobject_cast<QInputDialog*>(QApplication::activeModalWidget())){input->setTextValue("Native saved 日本語");input->accept();}});dialog->findChild<QPushButton*>("presetSaveAs")->click();}
        if(action==8){if(auto input=qobject_cast<QInputDialog*>(QApplication::activeModalWidget())){input->setTextValue("Native saved 日本語");input->accept();}}
        if(action==9||action==10){
            const auto folder=QFileInfo(QString::fromUtf8(obs_get_module_binary_path(obs_current_module()))).absolutePath();const auto file=folder+"/fixture-share.obsplayingpreset";
            const bool previous=QApplication::testAttribute(Qt::AA_DontUseNativeDialogs);QApplication::setAttribute(Qt::AA_DontUseNativeDialogs,true);
            QTimer::singleShot(0,dialog,[file]{if(auto chooser=qobject_cast<QFileDialog*>(QApplication::activeModalWidget())){chooser->selectFile(file);QMetaObject::invokeMethod(chooser,"accept",Qt::QueuedConnection);}});
            dialog->findChild<QPushButton*>(action==9?"presetExport":"presetImport")->click();QApplication::setAttribute(Qt::AA_DontUseNativeDialogs,previous);
        }
        if(action==6){const auto folder=QFileInfo(QString::fromUtf8(obs_get_module_binary_path(obs_current_module()))).dir();if(!dialog->grab().save(folder.filePath("manager-dialog.png")))blog(LOG_WARNING,"[playing-next] Test capture failed: manager dialog");}
        if(action==4)dialog->close();break;
    }},Qt::QueuedConnection);
}
extern "C" __declspec(dllexport) int playing_source_test_read(char *buffer,int capacity) {
    std::lock_guard<std::mutex> lock(evidenceMutex);if(capacity<=evidence.size())return 0;
    std::memcpy(buffer,evidence.constData(),static_cast<size_t>(evidence.size()));buffer[evidence.size()]=0;return int(evidence.size());
}
namespace {
void captureForTest(obs_source_t *source,const QString &filename={}) {
    auto retained=std::shared_ptr<obs_source_t>(obs_source_get_ref(source),obs_source_release);
    QMetaObject::invokeMethod(service.dispatcher,[retained,filename]{
        try {
            oto::ObsVisualRenderer target;
            const bool black=filename.endsWith(QStringLiteral("-black.png")),white=filename.endsWith(QStringLiteral("-white.png"));
            const bool textOnly=filename.endsWith(QStringLiteral("-text-a.png"))||filename.endsWith(QStringLiteral("-text-b.png"));
            const bool transitionTest=filename.contains(QStringLiteral("-transition-"));
            auto image=target.renderOffscreen(QSize(obs_source_get_width(retained.get()),obs_source_get_height(retained.get())),[&retained,black,white,textOnly,transitionTest,&filename]{
                if(black||white){vec4 matte;vec4_set(&matte,white?1.f:0.f,white?1.f:0.f,white?1.f:0.f,1.f);gs_clear(GS_CLEAR_COLOR,&matte,0,0);}
                if(!textOnly&&!transitionTest)obs_source_video_render(retained.get());
                else {
                    // Diagnostic isolation uses the exact prepared text and
                    // draw path. Moving ornaments cannot make a scroll PASS.
                    if(std::strcmp(obs_source_get_id(retained.get()),"obs_playing_next_development"))throw std::invalid_argument("Text isolation requires native source");
                    auto *source=static_cast<Source*>(obs_obj_get_data(retained.get()));
                    const auto published=source->state->consumer->publication.read();
                    if(!published||(textOnly&&!published->normalFrame))throw std::runtime_error("Native diagnostic frame unavailable");
                    auto frame=std::make_shared<visual::TrackFrame>(*published);
                    if(textOnly) {
                    auto normal=std::make_shared<visual::normal::PreparedNormal>(*frame->normalFrame);
                    normal->background.reset();normal->solidRects.clear();normal->ornaments.clear();frame->normalFrame=normal;
                    }
                    if(transitionTest) {
                        frame->normalSample.scrollSeconds=0;frame->normalSample.decorationMilliseconds=0;frame->layerMotion={};
                        frame->normalSample.transition={};frame->normalSample.transitioning=true;frame->wholeTransition=true;
                        if(filename.endsWith("-base.png")){frame->wholeTransition=false;frame->normalSample.transitioning=false;}
                        else if(filename.endsWith("-zero.png"))frame->normalSample.transition.alpha=0;
                        else if(!filename.endsWith("-full.png")) {
                            frame->normalSample.transition.alpha=.5;
                            if(filename.endsWith("-slide.png"))frame->normalSample.transition.offsetYPercent=-32.5;
                            if(filename.endsWith("-zoom.png"))frame->normalSample.transition.scale=.93;
                        }
                    }
                    std::lock_guard<std::mutex> lock(service.graphicsMutex);
                    if(!service.renderer)service.renderer=std::make_unique<oto::ObsVisualRenderer>();
                    if(!service.transitionSurface)service.transitionSurface=std::make_unique<visual::TransitionSurface>();
                    visual::drawTrack(*service.renderer,frame,true,service.transitionSurface.get());
                }
            });
            capturePixels=0;for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x)if(image.pixelColor(x,y).alpha())++capturePixels;
            captureHash=QCryptographicHash::hash(QByteArray(reinterpret_cast<const char *>(image.constBits()),int(image.sizeInBytes())),QCryptographicHash::Sha256).toHex();
            if(!filename.isEmpty()) {
                // Matte diagnostics save the actual framebuffer RGB as opaque.
                // Lua's SRCALPHA blend also changes target alpha; unpremultiplying
                // that result would distort RGB and hide the real compositing.
                if(black||white){image.reinterpretAsFormat(QImage::Format_RGBA8888);for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x)image.scanLine(y)[x*4+3]=255;}
                const auto folder=QFileInfo(QString::fromUtf8(obs_get_module_binary_path(obs_current_module()))).dir();
                if(!image.save(folder.filePath(filename),"PNG"))throw std::runtime_error("Test PNG save failed");
            }
            ++captures;
            evidenceGui();
        }catch(const std::exception &error){blog(LOG_ERROR,"[playing-next] Test capture failed: %s",error.what());}
    },Qt::QueuedConnection);
}
}
extern "C" __declspec(dllexport) void playing_source_test_capture(obs_source_t *source) {
    captureForTest(source);
}
// Test-only output stays beside the isolated module. No arbitrary output paths.
extern "C" __declspec(dllexport) void playing_source_test_capture_png(obs_source_t *source,const char *filename) {
    if(!filename)return;
    const auto name=QString::fromUtf8(filename);
    if(name.isEmpty()||name.size()>120||!name.endsWith(QStringLiteral(".png")))return;
    for(const auto c:name)if(!((c>=QLatin1Char('a')&&c<=QLatin1Char('z'))
        ||(c>=QLatin1Char('0')&&c<=QLatin1Char('9'))||c==QLatin1Char('-')||c==QLatin1Char('.')))return;
    if(name.contains(QStringLiteral("..")))return;
    captureForTest(source,name);
}
extern "C" __declspec(dllexport) void playing_source_test_remote_fixture(const char *profilePath) {
    const auto path=QString::fromUtf8(profilePath);
    QMetaObject::invokeMethod(service.dispatcher,[path] {
        try {
            const auto identity=companion::createIdentity();
            service.fixture=std::make_unique<RemoteFixture>();auto &fixture=*service.fixture;
            fixture.credential=companion::newDeviceCredential();
            if(!fixture.sender.pair(fixture.credential)||!fixture.sender.listen(identity,"127.0.0.1",0))throw std::runtime_error("TLS fixture failed");
            companion::Profile profile;profile.identity=identity;profile.autoReconnect=true;
            profile.receiver=companion::ReceiverSettings{"127.0.0.1",fixture.sender.port(),"OBS Playing Companion",companion::certificatePin(identity.certificate),fixture.credential};
            companion::ProtectedStore(path).save(companion::encodeProfile(profile));
            fixture.core.start();fixture.core.observe({core::Playback::Playing,core::Metadata{u8"Remote 🎵", "DJ"}});fixture.sender.publish(fixture.core.snapshot());
            evidenceGui();
        }catch(const std::exception &error){blog(LOG_ERROR,"[playing-next] Remote fixture failed: %s",error.what());}
    },Qt::QueuedConnection);
}
extern "C" __declspec(dllexport) void playing_source_test_remote_publish(int action) {
    QMetaObject::invokeMethod(service.dispatcher,[action] {
        if(!service.fixture)return;auto &fixture=*service.fixture;
        if(action==1)fixture.core.observe({core::Playback::Playing,core::Metadata{"Remote next", "DJ"}});
        if(action==2)fixture.core.observe({core::Playback::Paused,std::nullopt});
        if(action==3)fixture.core.observe({core::Playback::Playing,core::Metadata{"Remote resumed", "DJ"}});
        if(action==4)fixture.sender.revoke(fixture.credential.id);
        else fixture.sender.publish(fixture.core.snapshot());
        evidenceGui();
    },Qt::QueuedConnection);
}
#endif


