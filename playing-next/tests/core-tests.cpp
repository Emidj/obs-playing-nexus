// SPDX-License-Identifier: MIT
#include "playing/core/playing-core.hpp"

#include <atomic>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace playing::core;
namespace {
int checks = 0;
void require(bool condition, const char *message)
{
    ++checks;
    if (!condition) throw std::runtime_error(message);
}

Observation song(std::string title = "Midnight City", std::string artist = "M83")
{
    return {Playback::Playing, Metadata{std::move(title), std::move(artist)}};
}

void lifecycle()
{
    PlayingCore core;
    const auto initial = core.snapshot();
    require(!initial->running && initial->revision == 0 && !initial->metadata,
        "Initial core must be stopped and empty");
    require(core.observe(song()) == Update::NotRunning, "Stopped core accepted observation");
    require(core.stop() == Update::Unchanged && core.snapshot() == initial,
        "Repeated stop published a new snapshot");
    require(core.start() == Update::Applied, "Core failed to start");
    const auto waiting = core.snapshot();
    require(waiting->running && waiting->playback == Playback::Waiting && waiting->revision == 1,
        "Start did not enter waiting");
    require(core.start() == Update::Unchanged && core.snapshot() == waiting,
        "Repeated start republished state");
    require(core.observe(song()) == Update::Applied, "First song not published");
    require(core.stop() == Update::Applied && !core.snapshot()->metadata,
        "Stop retained stale metadata");
    require(core.start() == Update::Applied && !core.snapshot()->metadata,
        "Restart revived old song");
    require(initial->revision == 0 && waiting->revision == 1 && !waiting->metadata,
        "Retained snapshots were modified");
}

void changesAndDuplicates()
{
    PlayingCore core;
    core.start();
    require(core.observe(song()) == Update::Applied, "Song rejected");
    const auto firstConsumer = core.snapshot();
    const auto secondConsumer = core.snapshot();
    require(firstConsumer == secondConsumer, "Consumers received separate state copies");
    bool duplicatesUnchanged = true;
    for (int i = 0; i < 10000; ++i)
        duplicatesUnchanged = duplicatesUnchanged && core.observe(song()) == Update::Unchanged;
    require(duplicatesUnchanged && core.snapshot() == firstConsumer,
        "Duplicate observations caused publication");
    require(core.observe(song("Midnight City", "Other artist")) == Update::Applied,
        "Artist-only edit was lost");
    require(core.observe(song("Other title", "Other artist")) == Update::Applied,
        "Title-only edit was lost");
    require(firstConsumer->metadata->title == "Midnight City"
        && firstConsumer->metadata->artist == "M83", "Old publication changed");
    require(core.observe({Playback::Paused, std::nullopt}) == Update::Applied,
        "Pause did not publish");
    require(core.snapshot()->running && !core.snapshot()->metadata,
        "Pause must clear metadata without stopping core");
    require(core.observe({Playback::Paused, std::nullopt}) == Update::Unchanged,
        "Repeated pause republished");
    require(core.observe(song()) == Update::Applied && core.snapshot()->metadata.has_value(),
        "Resume did not restore same song");
    require(core.observe({Playback::Unavailable, std::nullopt}) == Update::Applied
        && !core.snapshot()->metadata, "Disconnect retained stale song");
    require(core.observe({Playback::Waiting, std::nullopt}) == Update::Applied,
        "Reconnect waiting status lost");
    require(core.observe(song()) == Update::Applied, "Reconnect did not accept song");
}

void metadataBoundary()
{
    PlayingCore core;
    core.start();
    require(core.observe(song(u8"Jóga 🎵 日本語", u8"Björk")) == Update::Applied,
        "Valid Unicode rejected");
    require(core.snapshot()->metadata->title == u8"Jóga 🎵 日本語", "Unicode bytes modified");
    require(core.observe(song("Untitled", "")) == Update::Applied, "Missing artist rejected");
    require(core.observe(song("AC-DC - Thunder - Live", "AC-DC")) == Update::Applied,
        "Hyphenated metadata rejected");
    require(core.observe(song(std::string(PlayingCore::maxMetadataBytes, 'x'), "")) == Update::Applied,
        "Exact metadata limit rejected");
    const auto lastValid = core.snapshot();
    const std::vector<Observation> invalid = {
        song("", "Artist"),
        song(std::string(PlayingCore::maxMetadataBytes + 1, 'x')),
        song("Title", std::string(PlayingCore::maxMetadataBytes + 1, 'x')),
        song(std::string("A\0B", 3)),
        song("Line\nBreak"), song("Title", "Artist\tName"),
        song(std::string("\xc0\xaf", 2)), // Overlong UTF-8.
        song(std::string("\xed\xa0\x80", 3)), // Surrogate.
        song(std::string("\xf4\x90\x80\x80", 4)), // Beyond Unicode range.
        song(std::string("\xe2\x82", 2)), // Partial UTF-8.
        song(std::string("\x80", 1)), // Unattached continuation.
        song(std::string("\xc2\x85", 2)), // Control character.
        {Playback::Playing, std::nullopt},
        {Playback::Paused, Metadata{"Old title", "Old artist"}},
        {Playback::Waiting, Metadata{"Old title", "Old artist"}},
        {Playback::Unavailable, Metadata{"Old title", "Old artist"}},
        {static_cast<Playback>(999), std::nullopt}
    };
    for (const auto &input : invalid)
        require(core.observe(input) == Update::InvalidObservation && core.snapshot() == lastValid,
            "Invalid input changed accepted state");
}

void lifetimeAndThreadContract()
{
    SnapshotPtr retained;
    {
        PlayingCore temporary;
        temporary.start(); temporary.observe(song());
        retained = temporary.snapshot();
    }
    require(retained->metadata->title == "Midnight City", "Snapshot died with core");

    PlayingCore core;
    core.start();
    const auto before = core.snapshot();
    bool rejected = false;
    std::thread wrongOwner([&] {
        try { core.observe(song()); } catch (const std::logic_error &) { rejected = true; }
    });
    wrongOwner.join();
    require(rejected && core.snapshot() == before, "Foreign thread mutated core");

    std::atomic<bool> done{false}, inconsistent{false};
    std::atomic<unsigned> ready{0}, reads{0};
    std::vector<std::thread> readers;
    for (int n = 0; n < 4; ++n) readers.emplace_back([&] {
        // Ensure each consumer reads at least once before the writer begins.
        const auto first = core.snapshot();
        if (!first->running) inconsistent = true;
        ++reads; ++ready;
        while (!done.load()) {
            const auto state = core.snapshot();
            if (!state->running || (state->playback == Playback::Playing &&
                (!state->metadata || !state->track || state->trackRevision == 0
                    || !(*state->track == *state->metadata) || state->metadata->title != state->metadata->artist)))
                inconsistent = true;
            ++reads;
        }
    });
    while (ready.load() != 4) std::this_thread::yield();
    for (int i = 0; i < 1000; ++i) {
        const auto value = std::to_string(i);
        core.observe(song(value, value));
    }
    done = true;
    for (auto &reader : readers) reader.join();
    require(reads.load() >= 4 && !inconsistent.load(), "Readers observed torn publication");
    require(core.snapshot()->revision == 1001 && core.snapshot()->metadata->title == "999",
        "Writer sequence lost an update");
}
} // namespace

int main()
{
    try {
        lifecycle(); changesAndDuplicates(); metadataBoundary(); lifetimeAndThreadContract();
        std::cout << "PASS: " << checks << " core contract checks; lifecycle, Unicode/limits, "
            "deduplication, pause/reconnect, immutable snapshots and concurrent readers\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
