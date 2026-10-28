// SPDX-License-Identifier: MIT
#pragma once
#include "provider-registry.hpp"
#include <mutex>
#include <string_view>

namespace playing::providers::network {
inline constexpr std::size_t maxFrameBytes = 65536;
enum class MessageKind { Snapshot, Heartbeat };
struct Message {
    std::string stream;
    std::uint64_t sequence = 0;
    MessageKind kind = MessageKind::Snapshot;
    core::Observation observation;
};
// Binary framing includes its 4-byte big-endian payload length. Version 1.
// No pointer, local trackRevision, file bytes or credentials on this boundary.
std::optional<std::string> encode(const Message &message);
std::optional<Message> decode(std::string_view bytes);
Message fromSnapshot(const core::Snapshot &snapshot, std::string stream, std::uint64_t sequence);
enum class Ingress { Accepted, Invalid, WrongStream, Replayed, Disconnected, InvalidClock };

// Host-owned ingress for an authenticated transport (Companion phase 08).
// This class does NOT authenticate a peer or expose a listener. Host explicitly
// authorizes a fresh stream ID, delivers events and services an expiry deadline.
// Bounded latest-state slot, thread-safe ingress; no timers, workers or polling.
class Channel final {
public:
    explicit Channel(double timeoutSeconds = 15.0);
    double timeoutSeconds() const noexcept { return timeout_; }
    bool connect(std::string stream, double now);
    Ingress receive(std::string_view frame, double now);
    // Authenticated transport only: correlate a separately transferred image to
    // the latest complete snapshot, including across intervening heartbeats.
    bool acceptsArtwork(const std::string &stream, std::uint64_t snapshotSequence) const;
    bool updateArtwork(const std::string &stream, std::uint64_t snapshotSequence, const core::Artwork &artwork);
    bool expire(double now);
    void disconnect();
    Sample sample(std::uint64_t &cursor) const;
private:
    mutable std::mutex mutex_;
    const double timeout_;
    std::string stream_;
    std::string lastStream_;
    double lastActivity_ = 0;
    std::uint64_t sequence_ = 0, snapshotSequence_ = 0, revision_ = 1;
    Sample latest_{SampleKind::Unavailable, std::nullopt, ProviderIssue::ConnectionLost};
};
// One explicit ID/channel per authenticated remote source, fixed for this
// registry. Request Automatic only. Registry shares it across consumers.
Registration registerProvider(ProviderRegistry &registry, std::shared_ptr<Channel> channel,
    std::string id = "network");
} // namespace playing::providers::network
