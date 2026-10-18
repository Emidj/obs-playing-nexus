// SPDX-License-Identifier: MIT
#pragma once
// Test-only synthetic audited layout. Never compiled into Providers/Core.
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
class VirtualDjFixture {
public:
    VirtualDjFixture() {
        data_ = static_cast<unsigned char *>(VirtualAlloc(nullptr, 0x5000000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        if (!data_) throw std::runtime_error("fixture allocation");
        set<std::uint64_t>(0x4c1fc08, 4); set<std::uint64_t>(0x4c1fbf0, base() + 0x10000);
        for (unsigned i = 0; i < 4; ++i) {
            const auto deck = 0x10000 + i * 0x10000, song = deck + 0x1000, db = deck + 0x2000;
            set<std::uint64_t>(0x4ea51f0 + i * 8, base() + deck);
            type(deck, 0x8000, ".?AVCDeck@@"); type(song, 0x8400, ".?AVCSong@@");
            set<std::uint32_t>(deck + 0x80, i); set<unsigned char>(deck + 0xc0, 1);
            set<std::uint64_t>(deck + 0x508, base() + song); set<std::uint64_t>(song + 0xa0, base() + db);
            set<std::uint64_t>(db + 0x20, base() + deck + 0x3000); set<std::uint64_t>(db + 0x18, base() + deck + 0x4000);
            text(deck + 0x3000, "Track " + std::to_string(i + 1)); text(deck + 0x4000, u8"Björk");
        }
    }
    ~VirtualDjFixture() { if (data_) VirtualFree(data_, 0, MEM_RELEASE); }
    VirtualDjFixture(const VirtualDjFixture &) = delete;
    VirtualDjFixture &operator=(const VirtualDjFixture &) = delete;
    std::uint64_t base() const { return reinterpret_cast<std::uint64_t>(data_); }
    template<class T> void set(std::size_t offset, T value) { std::memcpy(data_ + offset, &value, sizeof(value)); }
    void text(std::size_t offset, const std::string &value) {
        std::memset(data_ + offset, 0, 2048); std::memcpy(data_ + offset, value.data(), value.size());
    }
    void master(unsigned slot) { set<std::uint64_t>(0x4c1fbf0, slot ? base() + slot * 0x10000 : 0); }
    void playing(unsigned slot, bool active) { set<unsigned char>(slot * 0x10000 + 0xc0, active ? 1 : 0); }
private:
    unsigned char *data_ = nullptr;
    void type(std::size_t object, std::size_t table, const char *name) {
        set<std::uint64_t>(object, base() + table); set<std::uint64_t>(table - 8, base() + table + 0x80);
        set<std::uint32_t>(table + 0x80, 1); set<std::uint32_t>(table + 0x8c, static_cast<std::uint32_t>(table + 0x100));
        set<std::uint32_t>(table + 0x94, static_cast<std::uint32_t>(table + 0x80));
        // Only the 160-byte RTTI area; don't overwrite adjacent descriptors.
        std::memset(data_ + table + 0x110, 0, 160); std::memcpy(data_ + table + 0x110, name, std::strlen(name));
    }
};
