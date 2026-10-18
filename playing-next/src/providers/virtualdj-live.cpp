// SPDX-License-Identifier: MIT
#include "playing/providers/virtualdj-live.hpp"
#include "playing/providers/virtualdj-history.hpp"
#include "media-artwork.hpp"
#include <condition_variable>
#include <mutex>
#include <thread>
#include <stdexcept>
namespace playing::providers {
struct VirtualDjLive::Worker {
    std::mutex mutex;
    std::condition_variable wake;
    std::thread thread;
    bool stopping = false, requested = false, busy = false, ready = false;
    Sample completed;
};
namespace {
Sample failure(virtualdj::ProcessIssue issue) {
    using virtualdj::ProcessIssue;
    switch (issue) {
    case ProcessIssue::Missing: return {SampleKind::Unavailable, {}, ProviderIssue::ProcessMissing};
    case ProcessIssue::Multiple: return {SampleKind::Unavailable, {}, ProviderIssue::ProcessMultiple};
    case ProcessIssue::Version: return {SampleKind::Unavailable, {}, ProviderIssue::UnsupportedVersion};
    case ProcessIssue::Architecture: return {SampleKind::Unavailable, {}, ProviderIssue::InvalidLayout};
    default: return {SampleKind::TemporaryFailure, {}, ProviderIssue::ProcessAccess};
    }
}
}
VirtualDjLive::VirtualDjLive(Request request, Opener opener) : request_(std::move(request)), opener_(std::move(opener)) {}
VirtualDjLive::~VirtualDjLive() { stop(); }
void VirtualDjLive::start() {
    if (request_.providerId != "virtualdj" || request_.mode != Mode::Automatic || !request_.historyPath.empty()
        || request_.encoding != HistoryEncoding::Automatic) throw std::invalid_argument("Invalid VirtualDJ live request");
    if (worker_) return;
    worker_ = std::make_unique<Worker>();
    try {
        worker_->thread = std::thread([this] {
            auto &worker = *worker_;
            virtualdj::Connection connection; std::optional<unsigned> previous;detail::VirtualDjCover cover;
            std::unique_lock<std::mutex> lock(worker.mutex);
            for (;;) {
                worker.wake.wait(lock, [&] { return worker.stopping || worker.requested; });
                if (worker.stopping) return; // Connection closes on the worker.
                worker.requested = false; worker.busy = true; lock.unlock();
                Sample result;
                try {
                    if (!connection.memory || !connection.memory->alive()) {
                        connection = {}; previous.reset(); connection = opener_ ? opener_() : virtualdj::discover();
                    }
                    if (connection.issue != virtualdj::ProcessIssue::None || !connection.memory) result = failure(connection.issue);
                    else {
                        const auto decoded = virtualdj::decode(*connection.memory, connection.module, connection.version);
                        if (!connection.memory->alive()) {
                            connection = {}; previous.reset(); result = failure(virtualdj::ProcessIssue::Missing);
                        } else if (decoded.issue == virtualdj::SelectionIssue::ChangedSnapshot) {
                            result = {SampleKind::TemporaryFailure, {}, ProviderIssue::SnapshotChanged};
                        } else if (decoded.issue != virtualdj::SelectionIssue::None) {
                            const auto issue = decoded.issue == virtualdj::SelectionIssue::UnverifiedVersion ? ProviderIssue::UnsupportedVersion
                                : decoded.issue == virtualdj::SelectionIssue::InvalidMetadata ? ProviderIssue::InvalidMetadata : ProviderIssue::InvalidLayout;
                            result = {SampleKind::Unavailable, {}, issue}; previous.reset(); connection = {};
                        } else {
                            const auto selection = virtualdj::select(decoded.frame, previous);
                            previous = selection.deck;
                            result = {SampleKind::Observation, selection.observation};
                            if(result.observation&&result.observation->metadata)cover.enrich(*result.observation->metadata);
                        }
                    }
                } catch (...) { result = {SampleKind::TemporaryFailure, {}, ProviderIssue::ReadError}; connection = {}; previous.reset(); }
                lock.lock(); worker.busy = false;
                if (!worker.stopping) { worker.completed = std::move(result); worker.ready = true; }
            }
        });
    } catch (...) { worker_.reset(); throw; }
}
void VirtualDjLive::stop() noexcept {
    if (!worker_) return;
    { std::lock_guard<std::mutex> lock(worker_->mutex); worker_->stopping = true; }
    worker_->wake.notify_one();
    if (worker_->thread.joinable()) worker_->thread.join();
    worker_.reset();
}
Sample VirtualDjLive::sample() {
    if (!worker_) throw std::logic_error("VirtualDJ live reader stopped");
    std::lock_guard<std::mutex> lock(worker_->mutex);
    if (worker_->ready) { worker_->ready = false; return std::move(worker_->completed); }
    if (!worker_->busy && !worker_->requested) { worker_->requested = true; worker_->wake.notify_one(); }
    return {SampleKind::Pending, {}};
}
Registration registerVirtualDj(ProviderRegistry &registry) {
    return registry.registerProvider({"virtualdj", "VirtualDJ", true, true}, [](const Request &request) -> std::unique_ptr<IDjProvider> {
        if (request.mode == Mode::History) return std::make_unique<VirtualDjHistory>(request);
        return std::make_unique<VirtualDjLive>(request);
    });
}
}
