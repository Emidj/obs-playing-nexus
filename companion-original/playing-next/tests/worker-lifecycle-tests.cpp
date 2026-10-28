// SPDX-License-Identifier: MIT
#include "../src/providers/worker-provider.hpp"
#include "playing/providers/provider-registry.hpp"
#include <windows.h>
#include <iostream>
#include <chrono>
using namespace playing::providers;
struct Metrics {
    std::atomic<unsigned> constructed{0}, read{0}, closed{0}, destroyed{0}, mode{0};
    std::atomic<bool> onWorker{true}; std::thread::id owner = std::this_thread::get_id();
    void threadCheck() { if (std::this_thread::get_id() == owner) onWorker = false; }
};
class Reader final : public detail::WorkerReader {
public:
    explicit Reader(Metrics &metrics) : metrics_(metrics), event_(CreateEventW(nullptr, FALSE, FALSE, nullptr)) {
        if (!event_) throw std::runtime_error("fixture event"); ++metrics_.constructed; metrics_.threadCheck();
    }
    ~Reader() override { close(); ++metrics_.destroyed; metrics_.threadCheck(); }
    Sample read(const std::atomic<bool> &) override {
        ++metrics_.read; metrics_.threadCheck();
        if (metrics_.mode == 2) throw std::runtime_error("fixture read failure");
        return {SampleKind::Observation, playing::core::Observation{playing::core::Playback::Playing, playing::core::Metadata{"Track", "Artist"}}};
    }
    void close() noexcept override { if (event_) { CloseHandle(event_); event_ = nullptr; ++metrics_.closed; metrics_.threadCheck(); } }
private: Metrics &metrics_; HANDLE event_;
};
RefreshResult collect(ProviderRegistry &registry, const Request &request) {
    for (unsigned i = 0; i < 500; ++i) { auto result = registry.refresh(request); if (result.result != Refresh::Pending) return result; std::this_thread::sleep_for(std::chrono::milliseconds(2)); }
    throw std::runtime_error("lifecycle timeout");
}
int main() {
    unsigned checks = 0; auto check = [&](bool ok) { if (!ok) throw std::runtime_error("check " + std::to_string(checks + 1)); ++checks; };
    try {
        Metrics metrics; ProviderRegistry registry; const Request request{"fixture"};
        registry.registerProvider({"fixture", "Worker resources", true, false}, [&](const Request &) {
            return std::make_unique<detail::WorkerProvider>([&]() -> std::unique_ptr<detail::WorkerReader> {
                auto reader = std::make_unique<Reader>(metrics);
                if (metrics.mode == 1) throw std::runtime_error("fixture partial factory");
                return reader;
            });
        });
        const auto a = registry.acquire(request), b = registry.acquire(request);
        check(metrics.constructed == 0); auto result = collect(registry, request);
        check(result.result == Refresh::Applied && metrics.constructed == 1); const auto retained = result.snapshot;
        const auto reads = metrics.read.load(); std::this_thread::sleep_for(std::chrono::milliseconds(40)); check(metrics.read == reads);
        metrics.mode = 2; result = collect(registry, request);
        check(result.result == Refresh::TemporaryFailure && result.issue == ProviderIssue::ReadError); check(result.snapshot == retained);
        check(metrics.closed == 1 && metrics.destroyed == 1); metrics.mode = 0;
        check(collect(registry, request).snapshot == retained); check(metrics.constructed == 2);
        check(registry.release(a.token)); check(metrics.closed == 1); check(registry.release(b.token)); check(metrics.closed == 2 && metrics.destroyed == 2);
        metrics.mode = 1; const auto c = registry.acquire(request); result = collect(registry, request);
        check(result.result == Refresh::TemporaryFailure); check(metrics.constructed == 3 && metrics.closed == 3 && metrics.destroyed == 3);
        metrics.mode = 0; check(collect(registry, request).snapshot->metadata->title == "Track"); check(registry.release(c.token));
        DWORD before = 0, after = 0; check(GetProcessHandleCount(GetCurrentProcess(), &before) != 0);
        for (unsigned i = 0; i < 40; ++i) { const auto token = registry.acquire(request); collect(registry, request); registry.release(token.token); }
        check(GetProcessHandleCount(GetCurrentProcess(), &after) != 0); check(after == before); check(metrics.closed == metrics.constructed && metrics.destroyed == metrics.constructed);
        check(metrics.onWorker);
        std::cout << checks << " worker lifecycle checks; handles " << before << " -> " << after << "; 40 cycles, no autonomous reads (fixture only)\n"; return 0;
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
