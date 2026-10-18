// SPDX-License-Identifier: MIT
#pragma once

#include "playing/core/playing-core.hpp"
#include <string>
#include <tuple>

namespace playing::providers {

enum class Mode { Automatic, History };
enum class HistoryEncoding { Automatic, Ansi, Utf16LE };

struct Request {
    std::string providerId;
    Mode mode = Mode::Automatic;
    // History only. Empty means automatic discovery. Adapters/host canonicalize
    // a chosen path before requesting it; the registry does not touch the disk.
    std::string historyPath;
    HistoryEncoding encoding = HistoryEncoding::Automatic;
    bool operator<(const Request &other) const noexcept
    {
        return std::tie(providerId, mode, historyPath, encoding)
            < std::tie(other.providerId, other.mode, other.historyPath, other.encoding);
    }
};

struct Descriptor {
    std::string id;
    std::string name;
    bool automatic = false;
    bool history = false;
    bool remote = false; // Explicit trusted ingress may preserve original DJ origin.
};

enum class SampleKind { NoChange, Observation, TemporaryFailure, Unavailable, Pending };
enum class ProviderIssue {
    None, MissingHistory, AmbiguousHistory, ReadError, NoRecords,
    DatabaseBusy, DatabaseFormat, DatabaseSchema, DependencyUnavailable,
    InvalidMetadata, ReadTimeout, ProcessMissing, ProcessMultiple, ProcessAccess,
    UnsupportedVersion, InvalidLayout, SnapshotChanged, ConnectionLost, AmbiguousSelection
};
struct Sample {
    SampleKind kind = SampleKind::NoChange;
    std::optional<core::Observation> observation;
    ProviderIssue issue = ProviderIssue::None;
};

class IDjProvider {
public:
    virtual ~IDjProvider() = default;
    // Owner thread only; no calls back into the registry. Start prepares the
    // reader, not a promise that a DJ application is connected or playing.
    // Factories must create inert objects; resource acquisition starts here.
    virtual void start() = 0;
    // Idempotent and safe after an incomplete/failed start. Release all handles,
    // subscriptions and transport owned by this instance before returning.
    virtual void stop() noexcept = 0;
    // Called once by the central host on an event or an explicit bounded poll,
    // never by a renderer or each consuming source. Selection/master/history
    // semantics and metadata normalization belong to the adapter. Work must be
    // bounded/nonblocking so the owner thread remains responsive.
    virtual Sample sample() = 0;
};

} // namespace playing::providers
