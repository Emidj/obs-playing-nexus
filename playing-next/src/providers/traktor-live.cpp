// SPDX-License-Identifier: MIT
#include "playing/providers/traktor-live.hpp"
#include "worker-provider.hpp"
#include "traktor-artwork.hpp"
#include <chrono>
#include <stdexcept>
namespace playing::providers::traktor {
namespace {
virtualdj::Connection discoverTraktor(){
    auto fourth=virtualdj::discoverExecutable(L"Traktor Pro 4.exe",[](const std::string &v){return acceptsVersion(v)&&v[0]=='4';});
    auto third=virtualdj::discoverExecutable(L"Traktor.exe",[](const std::string &v){return acceptsVersion(v)&&v[0]=='3';});
    using virtualdj::ProcessIssue;
    if(fourth.issue!=ProcessIssue::Missing&&third.issue!=ProcessIssue::Missing)return {ProcessIssue::Multiple,{},{},{}};
    return fourth.issue!=ProcessIssue::Missing?std::move(fourth):std::move(third);
}
Sample failed(virtualdj::ProcessIssue issue){
    using virtualdj::ProcessIssue;
    switch(issue){
    case ProcessIssue::Missing:return {SampleKind::Unavailable,{},ProviderIssue::ProcessMissing};
    case ProcessIssue::Multiple:return {SampleKind::Unavailable,{},ProviderIssue::ProcessMultiple};
    case ProcessIssue::Version:return {SampleKind::Unavailable,{},ProviderIssue::UnsupportedVersion};
    case ProcessIssue::Architecture:return {SampleKind::Unavailable,{},ProviderIssue::InvalidLayout};
    default:return {SampleKind::TemporaryFailure,{},ProviderIssue::ProcessAccess};
    }
}
class Reader final:public providers::detail::WorkerReader {
    Options options_;virtualdj::Connection connection_;std::unique_ptr<MemoryReader> reader_;std::optional<unsigned> previous_;bool automaticLibrary_=false;
    std::string coverKey_;core::Artwork cover_;std::chrono::steady_clock::time_point retry_{};
public:
    explicit Reader(Options options):options_(std::move(options)){
        automaticLibrary_=options_.libraryRoot.empty();
        if(options_.cacheRoot.empty())options_.cacheRoot=detail::defaultCache();
    }
    Sample read(const std::atomic<bool> &stopping) override{
        if(stopping)return {SampleKind::Pending,{}};
        if(!connection_.memory||!connection_.memory->alive()){
            close();connection_=options_.opener?options_.opener():discoverTraktor();
            if(connection_.issue!=virtualdj::ProcessIssue::None||!connection_.memory)return failed(connection_.issue);
            if(automaticLibrary_)options_.libraryRoot=detail::documentsRoot(connection_.version);
            reader_=std::make_unique<MemoryReader>(*connection_.memory,connection_.module,connection_.version);
        }
        const auto next=reader_->read(stopping);
        if(!connection_.memory->alive()){close();return failed(virtualdj::ProcessIssue::Missing);}
        if(next.issue==Issue::Searching)return {SampleKind::Pending,{}};
        if(next.issue==Issue::Changed)return {SampleKind::TemporaryFailure,{},ProviderIssue::SnapshotChanged};
        if(next.issue==Issue::Version)return {SampleKind::Unavailable,{},ProviderIssue::UnsupportedVersion};
        if(next.issue!=Issue::None)return {SampleKind::Unavailable,{},ProviderIssue::InvalidLayout};
        const auto selected=select(next.frame,previous_);
        if(selected.issue!=Issue::None)return {SampleKind::TemporaryFailure,{},ProviderIssue::AmbiguousSelection};
        previous_=selected.deck;auto observation=selected.observation;
        if(observation.metadata){
            const auto now=std::chrono::steady_clock::now();
            if(selected.coverKey!=coverKey_||now>=retry_){
                coverKey_=selected.coverKey;cover_=detail::loadCover(options_.libraryRoot,options_.cacheRoot,coverKey_);
                retry_=now+(cover_.status==core::ArtworkStatus::Ready?std::chrono::seconds(10):std::chrono::seconds(1));
            }
            observation.metadata->artwork=cover_;
        }
        return {SampleKind::Observation,std::move(observation)};
    }
    void close() noexcept override{reader_.reset();connection_={};previous_.reset();coverKey_.clear();cover_={};retry_={};}
};
}
std::unique_ptr<IDjProvider> makeLive(const Request &request,Options options){
    if(request.providerId!="traktor"||request.mode!=Mode::Automatic||!request.historyPath.empty()||request.encoding!=HistoryEncoding::Automatic
        ||(!options.libraryRoot.empty()&&!options.libraryRoot.is_absolute())||(!options.cacheRoot.empty()&&!options.cacheRoot.is_absolute()))throw std::invalid_argument("Invalid Traktor automatic request");
    return std::make_unique<providers::detail::WorkerProvider>([options=std::move(options)]{return std::make_unique<Reader>(options);});
}
Registration registerProvider(ProviderRegistry &registry){return registry.registerProvider({"traktor","Traktor Pro 3 / 4",true,false},[](const Request &r){return makeLive(r);});}
}
