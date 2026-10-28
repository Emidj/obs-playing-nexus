// SPDX-License-Identifier: MIT
#include "playing/providers/provider-registry.hpp"

#include <iostream>
#include <stdexcept>
#include <thread>
#include <utility>

using namespace playing::providers;
namespace {
int checks = 0;
void require(bool condition, const char *message)
{
    ++checks;
    if (!condition) throw std::runtime_error(message);
}

struct Control {
    unsigned created = 0, started = 0, sampled = 0, stopped = 0, destroyed = 0;
    bool failStart = false, failSample = false;
    Sample next;
    std::vector<Request> requests;
};

// Test fixture only. None of these factories is registered in production code.
class FixtureProvider final : public IDjProvider {
public:
    explicit FixtureProvider(std::shared_ptr<Control> control) : control_(std::move(control))
    {
        ++control_->created;
    }
    ~FixtureProvider() override { ++control_->destroyed; }
    void start() override
    {
        ++control_->started;
        if (control_->failStart) throw std::runtime_error("fixture start failure");
    }
    void stop() noexcept override { ++control_->stopped; }
    Sample sample() override
    {
        ++control_->sampled;
        if (control_->failSample) throw std::runtime_error("fixture read failure");
        return control_->next;
    }
private:
    std::shared_ptr<Control> control_;
};

Factory factory(const std::shared_ptr<Control> &control)
{
    return [control](const Request &request) {
        control->requests.push_back(request);
        return std::make_unique<FixtureProvider>(control);
    };
}

Sample song(std::string title = "Midnight City", std::string artist = "M83")
{
    return {SampleKind::Observation,
        playing::core::Observation{playing::core::Playback::Playing,
            playing::core::Metadata{std::move(title), std::move(artist)}}};
}

void registrationAndRequests()
{
    ProviderRegistry registry;
    auto control = std::make_shared<Control>();
    require(registry.descriptors().empty() && registry.activeRequests().empty(),
        "Registry invented built-in adapters");
    require(registry.acquire({"virtualdj"}).result == Acquisition::UnknownProvider,
        "Missing adapter accepted");
    require(registry.registerProvider({"VirtualDJ", "Fixture", true, true}, factory(control))
        == Registration::Invalid, "Noncanonical ID accepted");
    require(registry.registerProvider({"virtualdj", "", true, true}, factory(control))
        == Registration::Invalid, "Empty descriptor name accepted");
    require(registry.registerProvider({"virtualdj", "Fixture", false, false}, factory(control))
        == Registration::Invalid, "Descriptor without modes accepted");
    require(registry.registerProvider({"virtualdj", "Fixture", true, true}, {})
        == Registration::Invalid, "Missing factory accepted");
    require(registry.registerProvider({"virtualdj", "VirtualDJ fixture", true, true}, factory(control))
        == Registration::Registered, "Factory registration failed");
    require(registry.registerProvider({"virtualdj", "Replacement", true, true}, factory(control))
        == Registration::Duplicate, "Duplicate silently replaced factory");
    require(registry.registerProvider({"rekordbox", "rekordbox fixture", true, true}, factory(control))
        == Registration::Registered, "rekordbox fixture registration failed");
    require(registry.registerProvider({"serato", "Serato fixture", true, false}, factory(control))
        == Registration::Registered, "Serato fixture registration failed");
    require(control->created == 0 && control->started == 0 && control->sampled == 0,
        "Registration started background readers");
    require(registry.acquire({"serato", Mode::History, ""}).result == Acquisition::UnsupportedMode,
        "Unsupported Serato history accepted");
    require(registry.acquire({"virtualdj", Mode::Automatic, "history.txt"}).result
        == Acquisition::InvalidRequest, "Automatic request carried history configuration");
    require(registry.acquire({"virtualdj", static_cast<Mode>(99), ""}).result
        == Acquisition::InvalidRequest, "Invalid mode accepted");
    require(registry.acquire({"virtualdj", Mode::Automatic, "", HistoryEncoding::Ansi}).result
        == Acquisition::InvalidRequest, "Live mode accepted history encoding");
    require(registry.acquire({"virtualdj", Mode::History, "", static_cast<HistoryEncoding>(99)}).result
        == Acquisition::InvalidRequest, "Invalid encoding accepted");
    require(registry.acquire({"virtualdj", Mode::History, std::string("A\0B", 3)}).result
        == Acquisition::InvalidRequest, "NUL in history path accepted");
    require(registry.acquire({"virtualdj", Mode::History, std::string(4097, 'x')}).result
        == Acquisition::InvalidRequest, "Oversized path accepted");
    auto descriptors = registry.descriptors();
    descriptors[0].id = "changed";
    require(registry.descriptors()[0].id == "rekordbox", "Caller modified registered descriptor");
}

void sharingAndRelease()
{
    auto control = std::make_shared<Control>();
    ProviderRegistry registry;
    registry.registerProvider({"virtualdj", "Fixture", true, true}, factory(control));
    const Request request{"virtualdj"};
    const auto first = registry.acquire(request), second = registry.acquire(request);
    require(first.result == Acquisition::Acquired && second.result == Acquisition::Acquired,
        "Two consumers failed to attach");
    require(control->created == 1 && control->started == 1 && registry.consumerCount() == 2
        && registry.activeRequests().size() == 1, "Same request created duplicate readers");
    require(first.snapshot == second.snapshot && control->sampled == 0,
        "Acquisition sampled or duplicated initial snapshot");
    control->next = song();
    require(registry.refresh(request).result == Refresh::Applied && control->sampled == 1,
        "Central refresh did not sample once");
    const auto published = registry.snapshot(first.token);
    require(published == registry.snapshot(second.token) && published->metadata->artist == "M83",
        "Consumers did not share published state");
    for (int i = 0; i < 1000; ++i) registry.snapshot(second.token);
    require(control->sampled == 1, "Reading snapshots polled provider");
    require(registry.refresh(request).result == Refresh::Unchanged
        && registry.snapshot(first.token) == published, "Duplicate song republished core state");
    require(registry.release(first.token) && control->stopped == 0,
        "One consumer stopped a shared reader");
    require(!registry.release(first.token) && !registry.snapshot(first.token),
        "Released token stayed valid");
    require(registry.release(second.token) && control->stopped == 1 && control->destroyed == 1,
        "Last consumer did not stop/destroy provider exactly once");
    require(registry.refresh(request).result == Refresh::UnknownSession && control->sampled == 2,
        "Inactive session continued sampling");
    require(published->metadata->title == "Midnight City", "Release invalidated retained snapshot");
    const auto again = registry.acquire(request);
    require(control->created == 2 && !again.snapshot->metadata,
        "New session reused stale metadata or dead reader");
    registry.clear();
    require(control->stopped == 2 && !registry.snapshot(again.token)
        && registry.consumerCount() == 0 && registry.activeRequests().empty(), "Clear leaked session");
    require(registry.descriptors().size() == 1, "Clear removed registered factories");
    const auto afterClear = registry.acquire(request);
    require(afterClear.result == Acquisition::Acquired && !registry.snapshot(again.token),
        "Clear reused an old token");
}

void independentRequests()
{
    auto control = std::make_shared<Control>();
    ProviderRegistry registry;
    registry.registerProvider({"rekordbox", "Fixture", true, true}, factory(control));
    const Request automatic{"rekordbox"};
    const Request discovered{"rekordbox", Mode::History, ""};
    const Request pathA{"rekordbox", Mode::History, u8"C:/Música/A.db"};
    const Request pathB{"rekordbox", Mode::History, u8"C:/Música/B.db"};
    const auto autoToken = registry.acquire(automatic), historyToken = registry.acquire(discovered);
    const auto a = registry.acquire(pathA), b = registry.acquire(pathB), sameA = registry.acquire(pathA);
    require(control->created == 4 && registry.activeRequests().size() == 4,
        "Different modes/paths were merged");
    require(control->requests[2].historyPath == pathA.historyPath, "Unicode path not preserved");
    control->next = song("Path A", "Artist");
    registry.refresh(pathA);
    require(registry.snapshot(a.token) == registry.snapshot(sameA.token), "Exact path not shared");
    require(!registry.snapshot(b.token)->metadata && !registry.snapshot(autoToken.token)->metadata
        && !registry.snapshot(historyToken.token)->metadata, "Other session received A's song");
}

void errorsAndReconnect()
{
    auto control = std::make_shared<Control>();
    ProviderRegistry registry;
    registry.registerProvider({"serato", "Fixture", true, false}, factory(control));
    const Request request{"serato"};
    const auto acquired = registry.acquire(request);
    control->next = song(u8"Jóga", u8"Björk"); registry.refresh(request);
    const auto original = registry.snapshot(acquired.token);
    control->next = {SampleKind::TemporaryFailure, std::nullopt};
    require(registry.refresh(request).result == Refresh::TemporaryFailure
        && registry.snapshot(acquired.token) == original, "Transient error cleared accepted song");
    control->failSample = true;
    require(registry.refresh(request).result == Refresh::ProviderException
        && registry.snapshot(acquired.token) == original, "Read exception changed state");
    control->failSample = false;
    const std::vector<Sample> invalid{
        {SampleKind::Observation, std::nullopt},
        {SampleKind::NoChange, song().observation},
        {SampleKind::TemporaryFailure, song().observation},
        {SampleKind::Unavailable, song().observation},
        {static_cast<SampleKind>(99), std::nullopt},
        {SampleKind::NoChange, std::nullopt, static_cast<ProviderIssue>(99)},
        {SampleKind::Pending, std::nullopt, ProviderIssue::ReadError},
        song("", "Artist"), song(std::string("\xff", 1), "Artist"),
        {SampleKind::Observation,
            playing::core::Observation{playing::core::Playback::Paused,
                playing::core::Metadata{"Stale", "Artist"}}}
    };
    for (const auto &sample : invalid) {
        control->next = sample;
        require(registry.refresh(request).result == Refresh::InvalidSample
            && registry.snapshot(acquired.token) == original, "Malformed sample changed core");
    }
    control->next = {SampleKind::NoChange, std::nullopt};
    require(registry.refresh(request).result == Refresh::Unchanged
        && registry.snapshot(acquired.token) == original, "No-change sample published");
    control->next = {SampleKind::Observation,
        playing::core::Observation{playing::core::Playback::Paused, std::nullopt}};
    require(registry.refresh(request).result == Refresh::Applied
        && !registry.snapshot(acquired.token)->metadata, "Pause retained song");
    control->next = {SampleKind::Unavailable, std::nullopt};
    require(registry.refresh(request).result == Refresh::Unavailable
        && registry.snapshot(acquired.token)->playback == playing::core::Playback::Unavailable,
        "Disconnect status missing");
    const auto disconnected = registry.snapshot(acquired.token);
    registry.refresh(request);
    require(registry.snapshot(acquired.token) == disconnected, "Repeated disconnect republished");
    control->next = song(u8"Jóga", u8"Björk");
    require(registry.refresh(request).result == Refresh::Applied && control->created == 1,
        "Reconnect failed or allocated another reader");
}

void failedStartsAndOwnership()
{
    auto control = std::make_shared<Control>();
    ProviderRegistry registry;
    registry.registerProvider({"null", "Null fixture", true, false},
        [](const Request &) { return std::unique_ptr<IDjProvider>{}; });
    registry.registerProvider({"throw", "Throw fixture", true, false},
        [](const Request &) -> std::unique_ptr<IDjProvider> { throw std::runtime_error("factory"); });
    require(registry.acquire({"null"}).result == Acquisition::FactoryFailed,
        "Null factory result accepted");
    require(registry.acquire({"throw"}).result == Acquisition::FactoryFailed,
        "Factory exception escaped");
    registry.registerProvider({"serato", "Fixture", true, false}, factory(control));
    control->failStart = true;
    const auto failed = registry.acquire({"serato"});
    require(failed.result == Acquisition::StartFailed && !failed.token && !failed.snapshot,
        "Partial start returned usable token");
    require(control->stopped == 1 && control->destroyed == 1 && registry.activeRequests().empty()
        && registry.consumerCount() == 0, "Failed start leaked resources/session");
    control->failStart = false;
    const auto working = registry.acquire({"serato"});
    require(working.result == Acquisition::Acquired, "Recovery from failed start failed");
    ProviderRegistry other;
    other.registerProvider({"serato", "Fixture", true, false}, factory(control));
    const auto otherToken = other.acquire({"serato"});
    require(!other.release(working.token) && !other.snapshot(working.token)
        && !registry.snapshot(otherToken.token), "Token crossed registry ownership");
    bool rejected = false;
    std::thread wrongOwner([&] {
        try { registry.refresh({"serato"}); } catch (const std::logic_error &) { rejected = true; }
    });
    wrongOwner.join();
    require(rejected && control->sampled == 0, "Foreign thread sampled provider");
}

void limitsAndDestruction()
{
    auto control = std::make_shared<Control>();
    {
        ProviderRegistry registry;
        for (std::size_t i = 0; i < ProviderRegistry::maxProviders; ++i)
            registry.registerProvider({"fixture" + std::to_string(i), "Fixture", true, true}, factory(control));
        require(registry.registerProvider({"extra", "Fixture", true, true}, factory(control))
            == Registration::CapacityExceeded, "Unbounded registration accepted");
        for (std::size_t i = 0; i < ProviderRegistry::maxSessions; ++i)
            registry.acquire({"fixture0", Mode::History, "path" + std::to_string(i)});
        const auto before = control->created;
        require(registry.acquire({"fixture0", Mode::History, "extra"}).result
            == Acquisition::CapacityExceeded && control->created == before,
            "Session limit started extra reader");
        registry.clear();
        require(control->stopped == ProviderRegistry::maxSessions, "Clear omitted session stops");
        for (std::size_t i = 0; i < ProviderRegistry::maxConsumers; ++i)
            registry.acquire({"fixture0"});
        require(registry.consumerCount() == ProviderRegistry::maxConsumers,
            "Consumers within limit rejected");
        require(registry.acquire({"fixture0"}).result == Acquisition::CapacityExceeded,
            "Consumer limit exceeded");
        require(registry.activeRequests().size() == 1, "Many consumers created many readers");
    }
    require(control->stopped == control->created && control->destroyed == control->created,
        "Registry destruction left providers alive");
}
} // namespace

int main()
{
    try {
        registrationAndRequests(); sharingAndRelease(); independentRequests();
        errorsAndReconnect(); failedStartsAndOwnership(); limitsAndDestruction();
        std::cout << "PASS: " << checks << " provider contract checks; registration, shared lifecycle, "
            "configuration isolation, errors/reconnect, ownership and capacity limits (fixtures only)\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
