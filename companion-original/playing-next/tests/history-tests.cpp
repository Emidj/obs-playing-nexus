// SPDX-License-Identifier: MIT
#include "playing/providers/virtualdj-history.hpp"
#include <windows.h>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>
#include <mutex>
#include <stdexcept>
#include <thread>

using namespace playing::providers;
namespace fs = std::filesystem;
namespace {
int checks = 0;
void require(bool condition, const char *why) { ++checks; if (!condition) throw std::runtime_error(why); }
void write(const fs::path &file, const std::string &bytes, bool append = false)
{
    std::ofstream output(file, std::ios::binary | (append ? std::ios::app : std::ios::trunc));
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!output) throw std::runtime_error("fixture write failed");
}
std::string read(const fs::path &file)
{
    std::ifstream input(file, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), {});
}
std::string utf16(const std::u16string &text, bool bom = true)
{
    std::string result = bom ? std::string("\xff\xfe", 2) : std::string{};
    for (const auto c : text) {
        result.push_back(static_cast<char>(c & 255));
        result.push_back(static_cast<char>(c >> 8));
    }
    return result;
}
RefreshResult complete(ProviderRegistry &registry, const Request &request)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    for (;;) {
        const auto result = registry.refresh(request);
        if (result.result != Refresh::Pending) return result;
        if (std::chrono::steady_clock::now() >= deadline) throw std::runtime_error("history worker timed out");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
std::string path(const fs::path &value) { return value.u8string(); }

void realFiles(const fs::path &root)
{
    const auto file = root / fs::u8path(u8"Música-historial.txt");
    const std::string original = u8"History - session\r\n21:03 : M83 - Midnight City\r\n21:05:12 - Björk - Jóga 🎵\r\n";
    write(file, original);
    ProviderRegistry registry;
    require(registerVirtualDjHistory(registry) == Registration::Registered, "Real history factory not registered");
    require(registry.descriptors().size() == 1 && !registry.descriptors()[0].automatic
        && registry.descriptors()[0].history, "Unported automatic reader advertised");
    require(registry.acquire({"virtualdj"}).result == Acquisition::UnsupportedMode,
        "Automatic mode accepted by history-only adapter");
    const Request request{"virtualdj", Mode::History, path(file)};
    const auto a = registry.acquire(request), b = registry.acquire(request);
    require(a.result == Acquisition::Acquired && b.result == Acquisition::Acquired
        && registry.activeRequests().size() == 1, "History consumers did not share session");
    require(complete(registry, request).result == Refresh::Applied, "Real file did not publish song");
    const auto state = registry.snapshot(a.token);
    require(state == registry.snapshot(b.token) && state->metadata->title == u8"Jóga 🎵"
        && state->metadata->artist == u8"Björk", "Last Unicode record incorrect");
    require(read(file) == original, "Reader modified history file");
    require(complete(registry, request).result == Refresh::Unchanged
        && registry.snapshot(a.token) == state, "Unchanged file republished");
    write(file, "21:06 : AC-DC - Thunder - Live", true);
    require(complete(registry, request).result == Refresh::Unchanged, "Partial record consumed");
    write(file, "\r\n", true);
    require(complete(registry, request).result == Refresh::Applied
        && registry.snapshot(a.token)->metadata->title == "Thunder - Live", "Completed append lost/hyphens split");
    write(file, "25:99 : A - Invalid\nHistory - new header\n", true);
    const auto before = registry.snapshot(a.token);
    require(complete(registry, request).result == Refresh::Unchanged
        && registry.snapshot(a.token) == before, "Invalid lines replaced song");
    write(file, "21:07 : Untitled\n", true);
    complete(registry, request);
    require(registry.snapshot(a.token)->metadata->artist.empty(), "Missing artist rejected");
    write(file, "21:08 : Artist\t-\tTabbed title\n", true);
    complete(registry, request);
    require(registry.snapshot(a.token)->metadata->title == "Tabbed title", "Whitespace separator differs from Lua");
    write(file, std::string("\xef\xbb\xbf", 3) + "21:09 : A - Appended BOM\n", true);
    complete(registry, request);
    require(registry.snapshot(a.token)->metadata->title == "Appended BOM", "Per-record BOM differs from Lua parser");
    write(file, "History - empty session\n");
    const auto retained = registry.snapshot(a.token);
    const auto empty = complete(registry, request);
    require(empty.result == Refresh::Unchanged && empty.issue == ProviderIssue::NoRecords
        && registry.snapshot(a.token) == retained, "Empty session unexpectedly cleared historical song");
    fs::remove(file); // Only this newly created fixture file; no recursive cleanup.
    const auto missing = complete(registry, request);
    require(missing.result == Refresh::Unavailable && missing.issue == ProviderIssue::MissingHistory
        && !registry.snapshot(a.token)->metadata, "Missing history did not clear stale metadata");
    write(file, original);
    require(complete(registry, request).result == Refresh::Applied
        && registry.snapshot(a.token)->metadata->title == u8"Jóga 🎵", "Recreated history failed to reconnect");
    registry.release(a.token);
    require(complete(registry, request).result == Refresh::Unchanged, "First release closed shared history");
    registry.release(b.token);
    require(registry.refresh(request).result == Refresh::UnknownSession, "Last release retained worker session");
}

void encodingsAndTail(const fs::path &root)
{
    ProviderRegistry registry;
    registerVirtualDjHistory(registry);
    const auto file = root / "encodings.txt";
    const Request automatic{"virtualdj", Mode::History, path(file)};
    const auto token = registry.acquire(automatic).token;
    write(file, std::string("\xef\xbb\xbf", 3) + u8"01:01 : A - Café\n");
    complete(registry, automatic);
    require(registry.snapshot(token)->metadata->title == u8"Café", "UTF-8 BOM not handled");
    write(file, utf16(u"01:02 : Björk - Jóga 🎵\r\n"));
    complete(registry, automatic);
    require(registry.snapshot(token)->metadata->title == u8"Jóga 🎵", "UTF-16 BOM/emoji not decoded");
    write(file, utf16(u"01:03 : A - Partial", true) + std::string(1, 'x'));
    const auto previous = registry.snapshot(token);
    require(complete(registry, automatic).result == Refresh::Unchanged
        && registry.snapshot(token) == previous, "Odd trailing UTF-16 byte consumed incomplete record");
    registry.release(token);
    const Request forced{"virtualdj", Mode::History, path(file), HistoryEncoding::Utf16LE};
    const auto forcedToken = registry.acquire(forced).token;
    write(file, utf16(u"01:04 : A - No BOM\n", false));
    complete(registry, forced);
    require(registry.snapshot(forcedToken)->metadata->title == "No BOM", "Forced UTF-16 missing BOM failed");
    const auto ordinaryToken = registry.acquire(automatic).token;
    require(registry.activeRequests().size() == 2, "Different encodings incorrectly shared session");
    registry.release(ordinaryToken); registry.release(forcedToken);
    const Request ansi{"virtualdj", Mode::History, path(file), HistoryEncoding::Ansi};
    const auto ansiToken = registry.acquire(ansi).token;
    write(file, "01:05 : A - ANSI\n");
    complete(registry, ansi);
    require(registry.snapshot(ansiToken)->metadata->title == "ANSI", "Explicit ANSI failed");
    write(file, std::string("\xef\xbb\xbf", 3) + u8"01:06 : A - 日本語\n");
    complete(registry, ansi);
    require(registry.snapshot(ansiToken)->metadata->title == u8"日本語", "UTF-8 BOM did not override ANSI selection");
    registry.release(ansiToken);
    const auto tailToken = registry.acquire(automatic).token;
    const std::string big = std::string(100000, 'x') + "\n01:07 : A - Bounded tail\n";
    write(file, big);
    complete(registry, automatic);
    require(registry.snapshot(tailToken)->metadata->title == "Bounded tail", "Bounded UTF-8 tail lost final record");
    std::u16string bigUtf16(100000, u'x'); bigUtf16 += u"\n01:08 : A - Unicode tail\n";
    write(file, utf16(bigUtf16));
    complete(registry, automatic);
    require(registry.snapshot(tailToken)->metadata->title == "Unicode tail", "Aligned UTF-16 tail lost final record");
    write(file, "01:09 : A - " + std::string(4097, 'z') + "\n");
    const auto accepted = registry.snapshot(tailToken);
    require(complete(registry, automatic).result == Refresh::Unchanged
        && registry.snapshot(tailToken) == accepted, "Oversized metadata replaced valid state");
}

void discoveryAndPending(const fs::path &root)
{
    const auto a = root / "discovery-a.txt", b = root / "discovery-b.txt";
    write(a, "02:01 : A - Found A\n"); write(b, "02:02 : B - Found B\n");
    const Request request{"virtualdj", Mode::History, ""};
    {
        ProviderRegistry registry;
        registry.registerProvider({"virtualdj", "Fixture discovery / real reader", false, true},
            [&](const Request &value) { return std::make_unique<VirtualDjHistory>(value,
                [&] { return std::vector<std::string>{path(a), path(b)}; }); });
        const auto token = registry.acquire(request).token;
        const auto result = complete(registry, request);
        require(result.result == Refresh::TemporaryFailure && result.issue == ProviderIssue::AmbiguousHistory
            && !registry.snapshot(token)->metadata, "Ambiguous discovery silently chose a file");
    }
    {
        ProviderRegistry registry;
        registry.registerProvider({"virtualdj", "Fixture discovery / real reader", false, true},
            [&](const Request &value) { return std::make_unique<VirtualDjHistory>(value,
                [&] { return std::vector<std::string>{path(a), path(a.parent_path() / "." / a.filename())}; }); });
        const auto token = registry.acquire(request).token;
        require(complete(registry, request).result == Refresh::Applied
            && registry.snapshot(token)->metadata->title == "Found A", "Duplicate path mistaken for ambiguity");
    }
    {
        ProviderRegistry registry;
        registry.registerProvider({"virtualdj", "Fixture discovery / real reader", false, true},
            [&](const Request &value) { return std::make_unique<VirtualDjHistory>(value,
                [&] { return std::vector<std::string>{path(root / "missing.txt")}; }); });
        const auto token = registry.acquire(request).token;
        require(complete(registry, request).issue == ProviderIssue::MissingHistory
            && !registry.snapshot(token)->metadata, "Missing discovery handled as a song");
    }
    {
        std::promise<void> entered;
        std::mutex mutex;
        std::condition_variable wake;
        bool released = false;
        auto signal = entered.get_future();
        ProviderRegistry registry;
        registry.registerProvider({"virtualdj", "Blocking fixture discovery / real reader", false, true},
            [&](const Request &value) { return std::make_unique<VirtualDjHistory>(value, [&] {
                entered.set_value();
                std::unique_lock<std::mutex> lock(mutex);
                wake.wait(lock, [&] { return released; });
                return std::vector<std::string>{path(a)};
            }); });
        const auto token = registry.acquire(request).token;
        const auto initial = registry.snapshot(token);
        require(registry.refresh(request).result == Refresh::Pending, "First async sample not pending");
        const bool workerEntered = signal.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
        // A blocked discovery must not block sample on the owner. Release gate
        // before assertion/cleanup so even a failed assertion can join safely.
        const auto pending = registry.refresh(request);
        { std::lock_guard<std::mutex> lock(mutex); released = true; }
        wake.notify_one();
        require(workerEntered && pending.result == Refresh::Pending && registry.snapshot(token) == initial,
            "Slow discovery blocked or modified owner-thread state");
        require(complete(registry, request).result == Refresh::Applied, "Async completion lost");
    }
    {
        VirtualDjHistory provider({"virtualdj", Mode::History, path(a)});
        provider.start(); provider.start(); provider.stop(); provider.stop();
        bool rejected = false;
        try { provider.sample(); } catch (const std::logic_error &) { rejected = true; }
        require(rejected, "Stopped reader still sampled");
        provider.start(); provider.stop();
    }
}
} // namespace

int main()
{
    try {
        const auto root = fs::current_path() / ("history-fixture-"
            + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        if (!fs::create_directory(root)) throw std::runtime_error("Fixture directory already exists");
        realFiles(root); encodingsAndTail(root); discoveryAndPending(root);
        std::cout << "PASS: " << checks << " native VirtualDJ history checks on real isolated files; "
            "Unicode/encoding, bounded tails, partial writes, discovery, reconnect and async lifecycle\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << '\n'; return 1;
    }
}
