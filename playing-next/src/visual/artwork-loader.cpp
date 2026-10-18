// SPDX-License-Identifier: MIT
#include "playing/visual/artwork-loader.hpp"
#include "native-https.hpp"
#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImageReader>
#include <QSvgRenderer>
#include <QXmlStreamReader>
#include <QPainter>
#include <QSet>
#include <QDir>
#include <QRegularExpression>
#include <QThread>
#include <QThreadPool>
#include <list>
#include <algorithm>
#include <map>
#include <stdexcept>

namespace playing::visual {
namespace {
struct Decoded { ImageOutcome outcome=ImageOutcome::Failed;oto::visual::ImagePtr image; };
std::string key(const core::ImageReference &ref) {
    return std::to_string(static_cast<int>(ref.kind))+":"+std::to_string(ref.location.size())+":"+ref.location+ref.version;
}
Decoded decode(core::ImageReference ref,const QString &root) {
    try {
        if(ref.kind==core::ImageKind::Asset) {
            const auto base=QFileInfo(root).canonicalFilePath();
            const auto path=QFileInfo(QDir(root).filePath(QString::fromUtf8(ref.location))).canonicalFilePath();
            const auto relative=QDir(base).relativeFilePath(path);
            if(base.isEmpty()||path.isEmpty()||relative.startsWith("../")||relative==".."||QDir::isAbsolutePath(relative))return {};
            ref.location=path.toUtf8().toStdString();
        }
        QFile file(QString::fromUtf8(ref.location.data(),static_cast<qsizetype>(ref.location.size())));
        constexpr qint64 limit=4*1024*1024;
        QByteArray bytes;
        if(ref.kind==core::ImageKind::Https){const auto reply=httpsGet(QUrl(QString::fromUtf8(ref.location)),{},limit);if(reply.status!=200||reply.body.isEmpty())return {};bytes=reply.body;}
        else{if(!QFileInfo(file).isFile())return {};if(!file.open(QIODevice::ReadOnly)||file.size()<1||file.size()>limit)return {};bytes=file.read(limit+1);if(bytes.size()>limit||file.error()!=QFileDevice::NoError)return {};}
        if(QFileInfo(file).suffix().compare("svg",Qt::CaseInsensitive)==0) {
            if(bytes.size()>256*1024)return {};
            QXmlStreamReader xml(bytes);int nodes=0;
            const QSet<QString> allowed={"svg","g","defs","rect","circle","ellipse","line","polyline","polygon","path","text","tspan"};
            while(!xml.atEnd()) {
                const auto token=xml.readNext();
                if(token==QXmlStreamReader::DTD||token==QXmlStreamReader::EntityReference||token==QXmlStreamReader::ProcessingInstruction)return {ImageOutcome::Unsupported,{}};
                if(!xml.isStartElement())continue;
                if(++nodes>1024||!allowed.contains(xml.name().toString()))return {ImageOutcome::Unsupported,{}};
                for(const auto &attribute:xml.attributes()) {
                    const auto name=attribute.name().toString().toLower(),value=attribute.value().toString().toLower();
                    if(name.startsWith("on")||name=="href"||value.contains("url(")||value.size()>16384)return {ImageOutcome::Unsupported,{}};
                }
            }
            if(xml.hasError())return {};
            QSvgRenderer svg(bytes);
            const auto size=svg.defaultSize();
            if(!svg.isValid()||svg.animated()||!size.isValid()||size.width()>4096||size.height()>4096||qint64(size.width())*size.height()>4*1024*1024)return {};
            QImage image(size.width()>1024||size.height()>1024?size.scaled(1024,1024,Qt::KeepAspectRatio):size,QImage::Format_RGBA8888_Premultiplied);
            image.fill(Qt::transparent);{QPainter painter(&image);svg.render(&painter);}
            return {ImageOutcome::Ready,std::make_shared<const oto::visual::ImageData>(oto::visual::ImageData{std::move(image)})};
        }
        QBuffer buffer(&bytes);buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer);reader.setAutoTransform(true);
        const auto format=reader.format().toLower();
        if(format!="png"&&format!="jpeg"&&format!="jpg"&&format!="webp")return {ImageOutcome::Unsupported,{}};
        const auto size=reader.size();
        if(!size.isValid()||size.width()>4096||size.height()>4096||qint64(size.width())*size.height()>4*1024*1024)return {};
        const auto target=size.scaled(1024,1024,Qt::KeepAspectRatio);
        if(size.width()>1024||size.height()>1024)reader.setScaledSize(target);
        auto image=reader.read();
        if(image.isNull()||image.width()>1024||image.height()>1024)return {};
        image=image.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
        if(image.isNull())return {};
        return {ImageOutcome::Ready,std::make_shared<const oto::visual::ImageData>(oto::visual::ImageData{std::move(image)})};
    } catch(...) {return {};}
}
}
struct ArtworkLoader::Impl {
    struct Request {
        std::uint64_t generation=0,track=0;
        std::optional<core::ImageReference> profile;
        bool fallback=false,usedProfile=false;
        Callback callback;
    };
    struct Waiter {Consumer consumer;std::uint64_t generation;};
    struct Job {std::uint64_t epoch;std::vector<Waiter> waiters;};
    struct Entry {std::string key;Decoded decoded;std::size_t bytes;};
    ArtworkLoader *owner;
    QThreadPool pool;
    std::map<Consumer,Request> requests;
    std::map<std::string,Job> jobs;
    std::list<Entry> cache;
    std::size_t maxBytes,maxEntries;
    QString assetRoot;
    std::uint64_t generation=0,epoch=0;
    ArtworkLoadStats counters;
    Impl(ArtworkLoader *o,std::size_t bytes,std::size_t entries):owner(o),maxBytes(bytes),maxEntries(entries) {
        pool.setMaxThreadCount(1);pool.setExpiryTimeout(0);
    }
    void thread() const {
        if(QThread::currentThread()!=owner->thread())throw std::logic_error("Artwork loader requires owner thread");
    }
    void deliver(Consumer consumer,std::uint64_t gen,Decoded decoded) {
        const auto found=requests.find(consumer);
        if(found==requests.end()||found->second.generation!=gen)return;
        auto &request=found->second;
        if(decoded.outcome!=ImageOutcome::Ready&&request.fallback&&!request.usedProfile&&request.profile) {
            request.usedProfile=true;
            acquire(consumer,gen,*request.profile);return;
        }
        const ArtworkResult result{decoded.outcome,decoded.image,request.track,request.usedProfile};
        QMetaObject::invokeMethod(owner,[this,consumer,gen,result] {
            const auto it=requests.find(consumer);
            if(it==requests.end()||it->second.generation!=gen)return;
            auto callback=std::move(it->second.callback);requests.erase(it);
            callback(result); // Last operation: callback may destroy the loader.
        },Qt::QueuedConnection);
    }
    void acquire(Consumer consumer,std::uint64_t gen,const core::ImageReference &ref) {
        const auto request=requests.find(consumer);if(request==requests.end()||request->second.generation!=gen)return;
        const bool asset=ref.kind==core::ImageKind::Asset&&!assetRoot.isEmpty()&&QRegularExpression("^[A-Za-z0-9_-]+\\.(png|svg)$").match(QString::fromUtf8(ref.location)).hasMatch();
        if(!core::validImageReference(ref)||(ref.kind!=core::ImageKind::LocalFile&&ref.kind!=core::ImageKind::Https&&!asset)) {
            deliver(consumer,gen,{ImageOutcome::Unsupported,{}});return;
        }
        const auto id=key(ref);
        for(auto it=cache.begin();it!=cache.end();++it)if(it->key==id) {
            const auto decoded=it->decoded;cache.splice(cache.begin(),cache,it);++counters.hits;
            deliver(consumer,gen,decoded);return;
        }
        auto pending=jobs.find(id);
        if(pending!=jobs.end()) {
            pending->second.waiters.push_back({consumer,gen});return;
        }
        // Includes obsolete jobs still draining after clear. No unbounded queue.
        if(jobs.size()>=8) {deliver(consumer,gen,{ImageOutcome::Capacity,{}});return;}
        jobs.emplace(id,Job{epoch,{{consumer,gen}}});++counters.reads;
        const auto jobEpoch=epoch;
        const auto root=assetRoot;
        pool.start([this,id,ref,jobEpoch,root] {
            const auto decoded=decode(ref,root);
            QMetaObject::invokeMethod(owner,[this,id,decoded,jobEpoch,ref] {
                auto job=jobs.find(id);if(job==jobs.end())return;
                auto waiters=std::move(job->second.waiters);jobs.erase(job);
                if(jobEpoch!=epoch){for(const auto &waiter:waiters)acquire(waiter.consumer,waiter.generation,ref);return;}
                const auto bytes=decoded.image?static_cast<std::size_t>(decoded.image->image.sizeInBytes()):0;
                if(maxEntries&&bytes<=maxBytes) {
                    cache.push_front({id,decoded,bytes});counters.bytes+=bytes;
                    while(cache.size()>maxEntries||counters.bytes>maxBytes) {
                        counters.bytes-=cache.back().bytes;cache.pop_back();++counters.evictions;
                    }
                }
                for(const auto &waiter:waiters)deliver(waiter.consumer,waiter.generation,decoded);
            },Qt::QueuedConnection);
        });
    }
};
ArtworkLoader::ArtworkLoader(std::size_t bytes,std::size_t entries,QString root):impl_(std::make_unique<Impl>(this,bytes,entries)) {
    if(!qobject_cast<QGuiApplication *>(QCoreApplication::instance())||QThread::currentThread()!=qApp->thread())
        throw std::logic_error("Artwork loader construction requires GUI thread");
    if(bytes>32*1024*1024||entries>128)throw std::invalid_argument("Artwork cache budget exceeds host limits");
    if(!root.isEmpty()&&!QFileInfo(root).isAbsolute())throw std::invalid_argument("Asset root must be absolute");
    impl_->assetRoot=std::move(root);
}
ArtworkLoader::~ArtworkLoader() {
    Q_ASSERT(QThread::currentThread()==thread());
    // Context stays alive until the worker has queued its final completion.
    // QObject destruction removes queued callbacks; no worker captures survive.
    impl_->pool.clear();impl_->pool.waitForDone();
}
void ArtworkLoader::request(Consumer consumer,core::SnapshotPtr snapshot,
    std::optional<core::ImageReference> profile,bool fallback,bool active,Callback callback) {
    impl_->thread();
    if(!callback)throw std::invalid_argument("Artwork callback required");
    cancel(consumer);
    if(!active||!snapshot||!snapshot->running||snapshot->playback!=core::Playback::Playing||!snapshot->track)return;
    if(!core::validMetadata(*snapshot->track))throw std::invalid_argument("Invalid artwork track");
    if(impl_->requests.size()>=128)throw std::length_error("Artwork consumer capacity exceeded");
    if(profile&&!core::validImageReference(*profile))profile.reset();
    const auto image=core::resolveArtwork(snapshot->track->artwork,profile,fallback);
    const bool usedProfile=snapshot->track->artwork.status!=core::ArtworkStatus::Ready&&image.has_value();
    const auto gen=++impl_->generation;
    impl_->requests.emplace(consumer,Impl::Request{gen,snapshot->trackRevision,profile,fallback,usedProfile,std::move(callback)});
    if(image)impl_->acquire(consumer,gen,*image);
    else impl_->deliver(consumer,gen,{ImageOutcome::Missing,{}});
}
void ArtworkLoader::cancel(Consumer consumer) {
    impl_->thread();impl_->requests.erase(consumer);
    for(auto &job:impl_->jobs) {
        auto &waiters=job.second.waiters;
        waiters.erase(std::remove_if(waiters.begin(),waiters.end(),[consumer](const auto &w){return w.consumer==consumer;}),waiters.end());
    }
}
void ArtworkLoader::clear() {
    impl_->thread();++impl_->epoch;impl_->requests.clear();impl_->cache.clear();impl_->counters.bytes=0;
    for(auto &job:impl_->jobs)job.second.waiters.clear();
}
ArtworkLoadStats ArtworkLoader::stats() const {
    impl_->thread();auto value=impl_->counters;value.entries=impl_->cache.size();value.pending=impl_->jobs.size();value.consumers=impl_->requests.size();return value;
}
}
