// SPDX-License-Identifier: MIT
#pragma once
#include "provider-registry.hpp"
namespace playing::providers {
struct SeratoOptions {
    bool advertise = true; // false only for isolated loopback fixtures.
    std::function<void(unsigned)> boundPort; // Diagnostics/test callback on worker.
};
std::unique_ptr<IDjProvider> makeSeratoLive(const Request &request, SeratoOptions options = {});
Registration registerSerato(ProviderRegistry &registry); // Automatic only, opt-in.
}
