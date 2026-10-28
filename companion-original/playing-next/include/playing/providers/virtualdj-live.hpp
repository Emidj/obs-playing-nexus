// SPDX-License-Identifier: MIT
#pragma once
#include "provider-registry.hpp"
#include "virtualdj-process.hpp"
namespace playing::providers {
class VirtualDjLive final : public IDjProvider {
public:
    using Opener = std::function<virtualdj::Connection()>;
    explicit VirtualDjLive(Request request, Opener opener = {});
    ~VirtualDjLive() override;
    void start() override;
    void stop() noexcept override;
    Sample sample() override;
private:
    Request request_;
    Opener opener_;
    struct Worker;
    std::unique_ptr<Worker> worker_;
};
// Unified explicit registration; keeps History and adds version-gated Automatic.
Registration registerVirtualDj(ProviderRegistry &registry);
}
