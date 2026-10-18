// SPDX-License-Identifier: MIT
#pragma once
#include "dj-provider.hpp"
#include <memory>
namespace playing::providers::serato {
class Transport final {
public:
    // false is for local fixtures only; production always advertises via Bonjour.
    explicit Transport(bool advertise = true);
    ~Transport();
    Transport(const Transport &) = delete;
    Transport &operator=(const Transport &) = delete;
    void start();
    void stop() noexcept;
    Sample step(double now);
    unsigned port() const noexcept;
    std::optional<std::string> mediaPath() const;
private:
    struct Impl;
    bool advertise_;
    std::unique_ptr<Impl> impl_;
};
}
