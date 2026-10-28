// SPDX-License-Identifier: MIT
#pragma once
#include "provider-registry.hpp"
#include <functional>

namespace playing::providers {

// Real Windows read-only history adapter derived from core.lua/windows.lua.
// Native automatic playback/master reading is not implemented by this class.
class VirtualDjHistory final : public IDjProvider {
public:
    using Discovery = std::function<std::vector<std::string>()>;
    explicit VirtualDjHistory(Request request, Discovery discovery = {});
    ~VirtualDjHistory() override;
    void start() override;
    void stop() noexcept override;
    Sample sample() override;
private:
    Request request_;
    Discovery discovery_;
    struct Worker;
    std::unique_ptr<Worker> worker_;
    std::string selectedPath_, signature_;
    Sample readHistory(); // Only the worker performs disk/discovery work.
};

// Explicit opt-in. Advertises History only, not the unported live reader.
Registration registerVirtualDjHistory(ProviderRegistry &registry);
} // namespace playing::providers
