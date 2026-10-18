// SPDX-License-Identifier: MIT
#pragma once
#include "playing/providers/dj-provider.hpp"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <stdexcept>
namespace playing::providers::detail {
class WorkerReader {
public:
    virtual ~WorkerReader() = default;
    virtual Sample read(const std::atomic<bool> &stopping) = 0;
    virtual void close() noexcept = 0;
};
// Inert factory; reader creation/read/close/destruction all happen on worker.
class WorkerProvider final : public IDjProvider {
public:
    using Factory = std::function<std::unique_ptr<WorkerReader>()>;
    explicit WorkerProvider(Factory factory) : factory_(std::move(factory)) {}
    ~WorkerProvider() override { stop(); }
    void start() override {
        if (worker_) return;
        worker_ = std::make_unique<Worker>();
        try { worker_->thread = std::thread([this] {
            auto &worker = *worker_; std::unique_ptr<WorkerReader> reader;
            std::unique_lock<std::mutex> lock(worker.mutex);
            for (;;) {
                worker.wake.wait(lock, [&] { return worker.stopping || worker.requested; });
                if (worker.stopping) { lock.unlock(); if (reader) reader->close(); return; }
                worker.requested = false; worker.busy = true; lock.unlock(); Sample result;
                try { if (!reader) reader = factory_(); if (!reader) throw std::runtime_error("Worker reader factory"); result = reader->read(worker.stopping); }
                catch (...) { if (reader) reader->close(); reader.reset(); result = {SampleKind::TemporaryFailure, {}, ProviderIssue::ReadError}; }
                lock.lock(); worker.busy = false;
                if (!worker.stopping) { worker.completed = std::move(result); worker.ready = true; }
            }
        }); } catch (...) { worker_.reset(); throw; }
    }
    void stop() noexcept override {
        if (!worker_) return;
        { std::lock_guard<std::mutex> lock(worker_->mutex); worker_->stopping = true; }
        worker_->wake.notify_one(); if (worker_->thread.joinable()) worker_->thread.join(); worker_.reset();
    }
    Sample sample() override {
        if (!worker_) throw std::logic_error("Provider stopped");
        std::lock_guard<std::mutex> lock(worker_->mutex);
        if (worker_->ready) { worker_->ready = false; return std::move(worker_->completed); }
        if (!worker_->busy && !worker_->requested) { worker_->requested = true; worker_->wake.notify_one(); }
        return {SampleKind::Pending, {}};
    }
private:
    struct Worker {
        std::mutex mutex; std::condition_variable wake; std::thread thread;
        std::atomic<bool> stopping{false}; bool requested = false, busy = false, ready = false; Sample completed;
    };
    Factory factory_; std::unique_ptr<Worker> worker_;
};
}
