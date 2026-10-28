// SPDX-License-Identifier: MIT
#include "playing/companion/detector.hpp"
#include "playing/providers/virtualdj-live.hpp"
#include "playing/providers/rekordbox-live.hpp"
#include "playing/providers/serato-live.hpp"
#include "playing/providers/traktor-live.hpp"
#include <QThread>
#include <QDir>
#include <QFileInfo>
#include <algorithm>
#include <stdexcept>
namespace playing::companion {
using namespace providers;
Detector::Detector(ProviderRegistry &registry,Sender &sender,QObject *parent)
    : QObject(parent),registry_(registry),sender_(sender),timer_(this),snapshot_(empty_.snapshot()) {
    registry_.descriptors(); // Verify owner before wiring any timer.
    timer_.setInterval(100); QObject::connect(&timer_,&QTimer::timeout,this,[this] {tick();});
}
Detector::~Detector() {changed={}; timer_.stop(); release();}
void Detector::requireOwner() const {
    if(QThread::currentThread()!=thread()) throw std::logic_error("Detector requires owner event loop");
}
bool Detector::select(std::optional<Request> request) {
    requireOwner();
    if(request) {
        const auto descriptors=registry_.descriptors(); const auto found=std::find_if(descriptors.begin(),descriptors.end(),[&](const auto &d) {return d.id==request->providerId && !d.remote;});
        if(found==descriptors.end() || request->historyPath.size()>4096 || request->historyPath.find('\0')!=std::string::npos) return false;
        if(request->mode==Mode::Automatic) {
            if(!found->automatic || !request->historyPath.empty() || request->encoding!=HistoryEncoding::Automatic) return false;
        } else if(request->mode==Mode::History) {
            if(!found->history || (request->encoding!=HistoryEncoding::Automatic && request->encoding!=HistoryEncoding::Ansi && request->encoding!=HistoryEncoding::Utf16LE)) return false;
            if(!request->historyPath.empty()) {
                const auto path=QString::fromUtf8(request->historyPath.data(),static_cast<qsizetype>(request->historyPath.size()));
                if(path.toUtf8().toStdString()!=request->historyPath || !QFileInfo(path).isAbsolute()) return false;
                request->historyPath=QDir::cleanPath(QDir::fromNativeSeparators(path)).toUtf8().toStdString();
            }
        } else return false;
    }
    selection_=std::move(request); if(active_) rebuild(); return true;
}
void Detector::setPreview(bool enabled) {requireOwner(); preview_=enabled; syncDemand();}
void Detector::release() {
    for(const auto &s:sessions_) registry_.release(s.token); sessions_.clear(); candidates_.clear();
}
void Detector::syncDemand() {
    requireOwner(); const bool wanted=preview_ || sender_.clientCount()!=0;
    if(wanted==active_) return; active_=wanted;
    if(active_) {empty_.start(); rebuild();}
    else {timer_.stop(); release(); empty_.stop(); issue_=ProviderIssue::None; publish(empty_.snapshot(),Detection::Inactive);}
}
void Detector::rebuild() {
    timer_.stop(); release(); issue_=ProviderIssue::None;
    std::vector<Request> requests;
    if(selection_) requests.push_back(*selection_);
    else for(const auto &d:registry_.descriptors()) if(d.automatic && !d.remote) requests.push_back({d.id,Mode::Automatic,""});
    bool failed=false;
    for(const auto &r:requests) {
        const auto result=registry_.acquire(r);
        if(result.result==Acquisition::Acquired) sessions_.push_back({r,result.token}); else failed=true;
    }
    acquisitionFailed_=failed;
    publish(empty_.snapshot(),failed?Detection::Error:Detection::Searching);
    if(!sessions_.empty()) {timer_.start(); tick();}
}
void Detector::publish(core::SnapshotPtr state,Detection status) {
    const bool differs=snapshot_!=state || status_!=status; snapshot_=std::move(state); status_=status;
    sender_.publish(snapshot_); if(differs && changed) changed();
}
void Detector::tick() {
    requireOwner(); if(!active_) return;
    const auto previousCandidates=candidates_; const auto previousIssue=issue_;
    const auto previousSnapshot=snapshot_; const auto previousStatus=status_;
    candidates_.clear(); std::vector<core::SnapshotPtr> playing;
    core::SnapshotPtr manual; issue_=ProviderIssue::None;
    for(const auto &session:sessions_) {
        const auto result=registry_.refresh(session.request);
        if(result.issue!=ProviderIssue::None) issue_=result.issue;
        if(selection_) manual=result.snapshot;
        if(result.snapshot && result.snapshot->playback==core::Playback::Playing) {candidates_.push_back(session.request.providerId); playing.push_back(result.snapshot);}
    }
    if(selection_ && manual) publish(manual,manual->playback==core::Playback::Playing || manual->playback==core::Playback::Paused?Detection::Selected:Detection::Searching);
    else if(playing.size()==1) publish(playing.front(),Detection::Selected);
    else publish(empty_.snapshot(),playing.empty()?(acquisitionFailed_?Detection::Error:Detection::Searching):Detection::Ambiguous);
    if(snapshot_==previousSnapshot && status_==previousStatus &&
       (previousCandidates!=candidates_ || previousIssue!=issue_) && changed) changed();
}
Detection Detector::status() const {requireOwner(); return status_;}
core::SnapshotPtr Detector::snapshot() const {requireOwner(); return snapshot_;}
std::vector<std::string> Detector::candidates() const {requireOwner(); return candidates_;}
ProviderIssue Detector::issue() const {requireOwner(); return issue_;}
bool registerNativeDetectors(ProviderRegistry &registry,const std::string &library) {
    if(!registry.descriptors().empty()) return false;
    if(rekordbox::registerProvider(registry,library)!=Registration::Registered) return false;
    return registerVirtualDj(registry)==Registration::Registered && registerSerato(registry)==Registration::Registered
        && traktor::registerProvider(registry)==Registration::Registered;
}
} // namespace playing::companion
