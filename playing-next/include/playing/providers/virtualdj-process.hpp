// SPDX-License-Identifier: MIT
#pragma once
#include "virtualdj-memory.hpp"
#include <memory>
#include <functional>
namespace playing::providers::virtualdj {
enum class ProcessIssue { None, Missing, Multiple, Access, Version, Architecture, Read };
class ProcessMemory : public Memory {
public:
    virtual bool alive() const noexcept = 0;
    virtual std::wstring imagePath() const = 0;
};
struct Connection {
    ProcessIssue issue = ProcessIssue::Missing;
    std::unique_ptr<ProcessMemory> memory;
    std::uint64_t module = 0;
    std::string version;
};
// Production discovery: exact executable name, strict resource version and x64.
Connection discover();
// Shared read-only Windows discovery for audited native readers. No writes.
Connection discoverExecutable(const wchar_t *executable,
    const std::function<bool(const std::string &)> &acceptVersion, std::uint32_t minimumImageSize = 0);
// Raw read-only transport, also used by isolated process tests. Does not certify
// executable identity, layout or version and never registers a provider.
std::unique_ptr<ProcessMemory> openReadOnly(std::uint32_t pid);
}
