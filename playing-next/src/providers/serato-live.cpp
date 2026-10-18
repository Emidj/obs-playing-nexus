// SPDX-License-Identifier: MIT
#include "playing/providers/serato-live.hpp"
#include "playing/providers/serato-transport.hpp"
#include "worker-provider.hpp"
#include "media-artwork.hpp"
#include <chrono>
namespace playing::providers {
namespace {
class Reader final : public detail::WorkerReader {
public:
    explicit Reader(SeratoOptions options) : options_(std::move(options)), transport_(options_.advertise) {}
    Sample read(const std::atomic<bool> &stopping) override {
        if (stopping) return {SampleKind::Pending, {}};
        if (!transport_.port()) {
            try { transport_.start(); if (options_.boundPort) options_.boundPort(transport_.port()); }
            catch (const std::runtime_error &error) {
                close(); return {SampleKind::Unavailable, {}, std::string(error.what()).find("Bonjour") != std::string::npos
                    ? ProviderIssue::DependencyUnavailable : ProviderIssue::ReadError};
            }
        }
        const auto now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        auto sample = transport_.step(now);
        if(sample.kind==SampleKind::Observation&&sample.observation&&sample.observation->metadata){
            auto &track=*sample.observation->metadata;const auto path=transport_.mediaPath().value_or("");
            const auto key=path+'\n'+track.title+'\n'+track.artist;
            const auto at=std::chrono::steady_clock::now();
            if(key!=coverKey_||at>=nextCover_){coverKey_=key;nextCover_=at+std::chrono::seconds(10);cover_={core::ArtworkStatus::Missing,{}};
                // Local drive paths only: no network/URI access from OSC telemetry.
                if(path.size()>3&&((path[0]>='A'&&path[0]<='Z')||(path[0]>='a'&&path[0]<='z'))&&path[1]==':'&&(path[2]=='/'||path[2]=='\\'))
                    if(const auto image=detail::extractMediaCover(path))cover_={core::ArtworkStatus::Ready,*image};
            }
            track.artwork=cover_;
        }
        if (sample.kind == SampleKind::Unavailable && sample.issue == ProviderIssue::DependencyUnavailable) close();
        return sample;
    }
    void close() noexcept override {
        transport_.stop(); if (options_.boundPort) { try { options_.boundPort(0); } catch (...) {} }
    }
private: SeratoOptions options_; serato::Transport transport_;
    std::string coverKey_;core::Artwork cover_;std::chrono::steady_clock::time_point nextCover_{};
};
}
std::unique_ptr<IDjProvider> makeSeratoLive(const Request &request, SeratoOptions options) {
    if (request.providerId != "serato" || request.mode != Mode::Automatic || !request.historyPath.empty()
        || request.encoding != HistoryEncoding::Automatic) throw std::invalid_argument("Invalid Serato request");
    return std::make_unique<detail::WorkerProvider>([options] { return std::make_unique<Reader>(options); });
}
Registration registerSerato(ProviderRegistry &registry) {
    return registry.registerProvider({"serato", "Serato", true, false}, [](const Request &request) { return makeSeratoLive(request); });
}
}
