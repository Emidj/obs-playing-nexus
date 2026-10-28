// SPDX-License-Identifier: MIT
// Local-only port of serato-live.lua. Public DNS-SD C ABI checked against
// Apple's dns_sd.h; no third-party header implementation copied here.
#include "playing/providers/serato-transport.hpp"
#include "playing/providers/serato-protocol.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace playing::providers::serato {
namespace {
using Ref = void *;
using RegistrationCallback = void (__stdcall *)(Ref, std::uint32_t, std::int32_t, const char *, const char *, const char *, void *);
using RecordCallback = void (__stdcall *)(Ref, Ref, std::uint32_t, std::int32_t, void *);
using Create = std::int32_t (__stdcall *)(Ref *);
using Record = std::int32_t (__stdcall *)(Ref, Ref *, std::uint32_t, std::uint32_t, const char *, std::uint16_t, std::uint16_t, std::uint16_t, const void *, std::uint32_t, RecordCallback, void *);
using Register = std::int32_t (__stdcall *)(Ref *, std::uint32_t, std::uint32_t, const char *, const char *, const char *, const char *, std::uint16_t, std::uint16_t, const void *, RegistrationCallback, void *);
using Sock = SOCKET (__stdcall *)(Ref);
using Process = std::int32_t (__stdcall *)(Ref);
using Deallocate = void (__stdcall *)(Ref);
template<class T> T symbol(HMODULE library, const char *name) {
    const auto address = GetProcAddress(library, name);
    if (!address) throw std::runtime_error("Bonjour entry point missing");
    T function{}; static_assert(sizeof(function) == sizeof(address), "Windows function pointer size");
    std::memcpy(&function, &address, sizeof(function)); return function;
}
void nonblocking(SOCKET socket) { u_long value = 1; if (ioctlsocket(socket, FIONBIO, &value)) throw std::runtime_error("Nonblocking socket"); }
bool tags(const Message &message, const char *pattern) {
    const std::string expected(pattern); if (expected.size() != message.arguments.size()) return false;
    for (std::size_t i = 0; i < expected.size(); ++i) {
        const auto &a = message.arguments[i];
        if ((expected[i] == 'b' && !std::holds_alternative<Blob>(a))
            || (expected[i] == 's' && !std::holds_alternative<std::string>(a))
            || (expected[i] == 'i' && !std::holds_alternative<std::int32_t>(a))) return false;
    }
    return true;
}
}
struct Transport::Impl {
    struct Peer { SOCKET socket = INVALID_SOCKET; std::string input, output; bool authorized = false, paired = false, subscribed = false, ended = false; double last = 0; };
    SOCKET listener = INVALID_SOCKET;
    bool wsa = false, dnsError = false, lost = false;
    unsigned boundPort = 0;
    HMODULE dns = nullptr;
    BCRYPT_ALG_HANDLE hash = nullptr;
    Create create = nullptr; Record record = nullptr; Register registration = nullptr;
    Sock socketOf = nullptr; Process process = nullptr; Deallocate deallocate = nullptr;
    std::vector<Ref> refs; std::vector<Peer> peers;
    std::optional<double> lastTraffic;
    State state;
    ~Impl() { close(); }
    static void __stdcall registered(Ref, std::uint32_t, std::int32_t error, const char *, const char *, const char *, void *context) {
        if (error) static_cast<Impl *>(context)->dnsError = true;
    }
    static void __stdcall recorded(Ref, Ref, std::uint32_t, std::int32_t error, void *context) {
        if (error) static_cast<Impl *>(context)->dnsError = true;
    }
    void drop(std::size_t i) noexcept {
        if (peers[i].paired) { state.clear(); lost = true; }
        closesocket(peers[i].socket); peers.erase(peers.begin() + i);
    }
    void close() noexcept {
        for (auto &peer : peers) {
            if (peer.paired) {
                try { const auto packet = encode({"/StreamMgmt/Pairing/UnPair", {}}); send(peer.socket, packet.data(), static_cast<int>(packet.size()), 0); }
                catch (...) {} // Best-effort unpair, never wait for network on stop.
            }
            closesocket(peer.socket);
        }
        peers.clear();
        if (listener != INVALID_SOCKET) { closesocket(listener); listener = INVALID_SOCKET; }
        if (deallocate) for (auto it = refs.rbegin(); it != refs.rend(); ++it) deallocate(*it);
        refs.clear();
        if (hash) { BCryptCloseAlgorithmProvider(hash, 0); hash = nullptr; }
        if (dns) { FreeLibrary(dns); dns = nullptr; }
        if (wsa) { WSACleanup(); wsa = false; }
        state.clear(); boundPort = 0;
    }
    void open(bool advertise) {
        refs.reserve(2); peers.reserve(4); // Avoid allocations after resource acquisition.
        if (advertise) {
            dns = LoadLibraryExW(L"dnssd.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
            if (!dns) throw std::runtime_error("Bonjour dependency unavailable");
            create = symbol<Create>(dns, "DNSServiceCreateConnection"); record = symbol<Record>(dns, "DNSServiceRegisterRecord");
            registration = symbol<Register>(dns, "DNSServiceRegister"); socketOf = symbol<Sock>(dns, "DNSServiceRefSockFD");
            process = symbol<Process>(dns, "DNSServiceProcessResult"); deallocate = symbol<Deallocate>(dns, "DNSServiceRefDeallocate");
        }
        WSADATA data{}; if (WSAStartup(MAKEWORD(2, 2), &data)) throw std::runtime_error("Winsock startup"); wsa = true;
        if (BCryptOpenAlgorithmProvider(&hash, BCRYPT_MD5_ALGORITHM, nullptr, 0) < 0) throw std::runtime_error("Serato digest unavailable");
        listener = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); if (listener == INVALID_SOCKET) throw std::runtime_error("Listener creation");
        nonblocking(listener); sockaddr_in address{}; address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (bind(listener, reinterpret_cast<sockaddr *>(&address), sizeof(address)) || listen(listener, 8)) throw std::runtime_error("Local listener bind");
        int length = sizeof(address); if (getsockname(listener, reinterpret_cast<sockaddr *>(&address), &length)) throw std::runtime_error("Listener port");
        boundPort = ntohs(address.sin_port);
        if (advertise) {
            Ref connection = nullptr;
            const auto status = create(&connection); if (connection) refs.push_back(connection);
            if (status || !connection) throw std::runtime_error("Bonjour connection");
            Ref registrationRecord = nullptr; const unsigned char ip[4]{127,0,0,1};
            const auto host="obs-playing-serato-"+std::to_string(boundPort)+".local.";
            const auto name="OBS Playing @ Local "+std::to_string(boundPort);
            if (record(connection, &registrationRecord, 32, UINT32_MAX, host.c_str(), 1, 1, 4, ip, 120, recorded, this)) throw std::runtime_error("Bonjour record");
            Ref service = nullptr;
            const auto registeredStatus = registration(&service, 0, UINT32_MAX, name.c_str(), "_SeratoIOSRemote._tcp", "local.",
                host.c_str(), address.sin_port, 0, nullptr, registered, this);
            if (service) refs.push_back(service);
            if (registeredStatus || !service) throw std::runtime_error("Bonjour registration: "+std::to_string(registeredStatus));
        }
    }
    void queue(Peer &peer, const Message &message) {
        const auto packet = encode(message);
        if (packet.size() > 65536 - peer.output.size()) throw std::runtime_error("Serato send queue limit");
        peer.output += packet;
    }
    void subscribe(Peer &peer) {
        if (peer.subscribed) return;
        for (const auto *topic : {"Deck/Song/Title", "Deck/Song/Artist", "Deck/Song/Filepath", "Deck/Song/Valid", "Deck/Playhead", "Video/Deck/Mixer/Upfader", "Video/Mixer/Crossfader"})
            queue(peer, {std::string("/Register/Status/") + topic, {}});
        peer.subscribed = true;
    }
    void message(Peer &peer, const Message &message, double now) {
        if (message.path == "/Ping") queue(peer, {"/Ping", {}});
        else if (message.path == "/StreamMgmt/Authorize/Request") {
            if (!tags(message, "bii") || std::get<Blob>(message.arguments[0]).size() != 16) throw std::runtime_error("Serato challenge");
            static const char *hex = "d25db261a4411bc1f3788f57723b8977c541a4b619b94a9a8b45c46f8511c2f8";
            auto input = std::get<Blob>(message.arguments[0]);
            for (unsigned i = 0; i < 64; i += 2) {
                auto digit = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
                input.push_back(static_cast<unsigned char>((digit(hex[i]) << 4) | digit(hex[i + 1])));
            }
            Blob digest(16);
            if (BCryptHash(hash, nullptr, 0, input.data(), static_cast<ULONG>(input.size()), digest.data(), 16) < 0) throw std::runtime_error("Serato digest");
            queue(peer, {"/StreamMgmt/Authorize/Response", {std::string("OBS Playing"), std::string("OBS Playing"), digest}}); peer.authorized = true;
        } else if (message.path == "/StreamMgmt/Pairing/Pair" && peer.authorized && tags(message, "ssi")) {
            // Single paired telemetry stream, plus separate heartbeat connections.
            if (!peer.paired && std::any_of(peers.begin(), peers.end(), [](const Peer &p) { return p.paired; })) throw std::runtime_error("Multiple Serato telemetry streams");
            queue(peer, {message.path, {std::string("OBS Playing"), std::string("OBS Playing"), std::int32_t(1)}}); peer.paired = true; subscribe(peer);
        } else if (message.path == "/StreamMgmt/Pairing/StatusChanged" && peer.paired) subscribe(peer);
        else if (message.path == "/StreamMgmt/Pairing/UnPair") peer.ended = true;
        else if (peer.paired && message.path.compare(0, 8, "/Status/") == 0) applyStatus(state, message, now);
    }
    bool service(Peer &peer, double now) {
        char bytes[65536]; const auto received = recv(peer.socket, bytes, sizeof(bytes), 0);
        if (!received || (received < 0 && WSAGetLastError() != WSAEWOULDBLOCK)) return false;
        if (received > 0) {
            peer.last = now; lastTraffic = now;
            auto frames = split(peer.input, std::string(bytes, static_cast<std::size_t>(received)));
            if (!frames.valid) return false; peer.input = std::move(frames.rest);
            for (const auto &frame : frames.frames) {
                const auto decoded = decode(frame); if (!decoded.valid) return false;
                for (const auto &m : decoded.messages) message(peer, m, now);
            }
        }
        if (!peer.output.empty()) {
            const auto sent = send(peer.socket, peer.output.data(), static_cast<int>(peer.output.size()), 0);
            if (sent < 0) { if (WSAGetLastError() != WSAEWOULDBLOCK) return false; }
            else peer.output.erase(0, static_cast<std::size_t>(sent));
        }
        return !peer.ended && now - lastTraffic.value_or(peer.last) < 45;
    }
    std::optional<std::string> selectedPath;
    Sample step(double now) {
        selectedPath.reset();
        if (!std::isfinite(now) || now < 0) return {SampleKind::TemporaryFailure, {}, ProviderIssue::ReadError};
        for (const auto ref : refs) {
            WSAPOLLFD descriptor{socketOf(ref), POLLRDNORM, 0};
            const auto ready = WSAPoll(&descriptor, 1, 0);
            if (ready < 0 || (ready > 0 && (!(descriptor.revents & POLLRDNORM) || process(ref)))) return {SampleKind::Unavailable, {}, ProviderIssue::DependencyUnavailable};
        }
        if (dnsError) return {SampleKind::Unavailable, {}, ProviderIssue::DependencyUnavailable};
        for (unsigned i = 0; i < 4; ++i) {
            sockaddr_in address{}; int size = sizeof(address);
            const auto client = accept(listener, reinterpret_cast<sockaddr *>(&address), &size);
            if (client == INVALID_SOCKET) break;
            if (peers.size() >= 4 || address.sin_addr.s_addr != htonl(INADDR_LOOPBACK)) { closesocket(client); continue; }
            try { nonblocking(client); Peer peer; peer.socket = client; peer.last = now; peers.push_back(std::move(peer)); }
            catch (...) { closesocket(client); }
        }
        for (auto i = peers.size(); i > 0; --i) {
            bool alive = false; try { alive = service(peers[i - 1], now); } catch (...) {}
            if (!alive) drop(i - 1);
        }
        const auto selected = state.read(now);
        if (lost) { lost = false; return {SampleKind::Unavailable, {}, ProviderIssue::ConnectionLost}; }
        if (selected.issue == StateIssue::AwaitingTelemetry) return {SampleKind::Pending, {}};
        if (selected.issue != StateIssue::None) return {SampleKind::TemporaryFailure, {}, ProviderIssue::InvalidMetadata};
        selectedPath=selected.mediaPath;
        return {SampleKind::Observation, selected.observation};
    }
};
Transport::Transport(bool advertise) : advertise_(advertise) {}
Transport::~Transport() { stop(); }
void Transport::start() {
    if (impl_) return;
    auto fresh = std::make_unique<Impl>(); fresh->open(advertise_); impl_ = std::move(fresh);
}
void Transport::stop() noexcept { impl_.reset(); }
Sample Transport::step(double now) { if (!impl_) throw std::logic_error("Serato transport stopped"); return impl_->step(now); }
unsigned Transport::port() const noexcept { return impl_ ? impl_->boundPort : 0; }
std::optional<std::string> Transport::mediaPath() const {return impl_?impl_->selectedPath:std::optional<std::string>{};}
}
