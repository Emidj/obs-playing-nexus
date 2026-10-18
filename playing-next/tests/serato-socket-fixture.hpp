// SPDX-License-Identifier: MIT
#pragma once
#include "playing/providers/serato-protocol.hpp"
#include <winsock2.h>
#include <stdexcept>
class SeratoClient {
public:
    explicit SeratoClient(unsigned port) {
        socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in address{}; address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(INADDR_LOOPBACK); address.sin_port = htons(static_cast<u_short>(port));
        if (socket_ == INVALID_SOCKET || connect(socket_, reinterpret_cast<sockaddr *>(&address), sizeof(address))) { close(); throw std::runtime_error("fixture connect"); }
        u_long value = 1; if (ioctlsocket(socket_, FIONBIO, &value)) { close(); throw std::runtime_error("fixture nonblocking"); }
    }
    ~SeratoClient() { close(); }
    SeratoClient(const SeratoClient &) = delete;
    SeratoClient &operator=(const SeratoClient &) = delete;
    void close() { if (socket_ != INVALID_SOCKET) { closesocket(socket_); socket_ = INVALID_SOCKET; } }
    void bytes(const std::string &data) {
        if (send(socket_, data.data(), static_cast<int>(data.size()), 0) != static_cast<int>(data.size())) throw std::runtime_error("fixture send");
    }
    void sendMessage(const playing::providers::serato::Message &message) { bytes(playing::providers::serato::encode(message)); }
    std::vector<playing::providers::serato::Message> receive() {
        using namespace playing::providers::serato;
        std::vector<Message> result;
        char buffer[65536]; const auto size = recv(socket_, buffer, sizeof(buffer), 0);
        if (size > 0) {
            const auto parts = split(rest_, std::string(buffer, static_cast<std::size_t>(size)));
            if (!parts.valid) throw std::runtime_error("fixture split"); rest_ = parts.rest;
            for (const auto &part : parts.frames) { const auto decoded = decode(part); if (!decoded.valid) throw std::runtime_error("fixture decode"); result.insert(result.end(), decoded.messages.begin(), decoded.messages.end()); }
        } else if (size < 0 && WSAGetLastError() != WSAEWOULDBLOCK) throw std::runtime_error("fixture recv");
        return result;
    }
    void authorize() { sendMessage({"/StreamMgmt/Authorize/Request", {playing::providers::serato::Blob(16, 1), std::int32_t(1), std::int32_t(1)}}); }
    void pair() { sendMessage({"/StreamMgmt/Pairing/Pair", {std::string("Fixture"), std::string("Fixture"), std::int32_t(1)}}); }
    void tracks() {
        using namespace playing::providers::serato;
        for (std::int32_t i = 0; i < 2; ++i) {
            sendMessage({"/Status/Deck/Song/Valid", {i, 1.f}});
            sendMessage({"/Status/Deck/Song/Title", {i, std::string(i ? "Track 2" : "Track 1")}});
            sendMessage({"/Status/Deck/Song/Artist", {i, std::string(u8"Björk")}});
            sendMessage({"/Status/Deck/Playhead", {i, 0.f, 1.f, 120.f}});
        }
    }
private: SOCKET socket_ = INVALID_SOCKET; std::string rest_;
};
