// SPDX-License-Identifier: MIT
#pragma once
#include "provider-registry.hpp"
#include <functional>

namespace playing::providers {
std::optional<std::string> discoverRekordboxDatabase(); // Worker-only, existing options/relocation rules.
struct RekordboxHistoryLocations {
    std::vector<std::string> optionsFiles;
    std::vector<std::string> databases;
};
// History only. DLL location is host supplied and must be absolute.
// All database handles and I/O belong to the sleeping session worker.
class RekordboxHistory final : public IDjProvider {
public:
    using Discovery = std::function<RekordboxHistoryLocations()>;
    RekordboxHistory(Request request, std::string libraryPath, Discovery discovery = {});
    ~RekordboxHistory() override;
    void start() override;
    void stop() noexcept override;
    Sample sample() override;
private:
    Request request_;
    std::string libraryPath_;
    Discovery discovery_;
    struct Worker;
    std::unique_ptr<Worker> worker_;
};
Registration registerRekordboxHistory(ProviderRegistry &registry, std::string libraryPath,
    RekordboxHistory::Discovery discovery = {});
} // namespace playing::providers
