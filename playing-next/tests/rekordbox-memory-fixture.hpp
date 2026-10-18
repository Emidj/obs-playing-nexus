// SPDX-License-Identifier: MIT
#pragma once
#include "playing/providers/virtualdj-memory.hpp"
#include <windows.h>
#include <cstring>
#include <map>
#include <functional>
#include <stdexcept>
class RekordboxFixture final : public playing::providers::virtualdj::Memory {
public:
    std::function<void(std::uint64_t)> hook;
    std::array<std::size_t, 4> positions{}, masters{}, texts{};
    explicit RekordboxFixture(unsigned shift = 0, bool legacy = false) : legacy_(legacy) {
        data_ = static_cast<unsigned char *>(VirtualAlloc(nullptr, size_, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        if (!data_) throw std::runtime_error("RB fixture allocation");
        text(0, "MZ"); put<std::uint32_t>(60, 0x80); text(0x80, std::string("PE\0\0\x64\x86\1\0", 8)); put<std::uint16_t>(0x94, 0xf0);
        const auto section = 0x80 + 24 + 0xf0; put<std::uint32_t>(section + 8, 0x1000); put<std::uint32_t>(section + 12, 0x1000); put<std::uint32_t>(section + 36, 0xc0000040);
        object(0x100000, "MainComponent"); object(0x110000, "UiManager@djplay"); object(0x120000, "PlayerComponent@djplay"); object(0x130000, "PlayerPanelL@PlayerComponent@djplay");
        pointer(0x1188 + shift, 0x100000); pointer(0x100000 + (legacy ? 0x648 : 0x488) + shift, 0x110000);
        pointer(0x110000 + 0xd0 + shift, 0x120000); pointer(0x120000 + 0x128 + shift, 0x130000);
        for (unsigned i = 0; i < 4; ++i) {
            const auto d = (i + 1) * 0x1000, view = 0x150000 + d, title = 0x160000 + d, widget = 0x190000 + d;
            const auto ui = 0x1a0000 + d, core = 0x1b0000 + d, control = 0x1c0000 + d, button = 0x1d0000 + d, value = 0x1e0000 + d;
            object(view, "TrackInfoView@djplay"); object(title, "TitleView@TrackInfoView@djplay");
            object(widget, "WidgetPlayer@PlayerComponent@djplay"); object(ui, "UiPlayer@djplay"); object(core, "WidgetPlayer@djplay");
            object(control, "PlayerControllPanel@djplay"); object(button, "DrawableButton@juce"); object(value, "SimpleValueSource@juce");
            pointer(0x130000 + 0x370 + shift + i * 8, view); pointer(view + 0x158 + shift, title);
            texts[i] = 0x180000 + d; pointer(title + 0xe8 + shift, texts[i]);
            text(texts[i], u8"Título de la pista: Canción " + std::to_string(i + 1) + u8"\nArtista: Björk\nÁlbum: Disco");
            pointer(view + 0x1e8 + shift, widget); pointer(widget + 8, ui); pointer(ui + 0x480 + shift, core);
            pointer(0x130000 + 0x280 + i * 8, control); pointer(control + (legacy ? 0x310 : 0x1b0) + shift, widget);
            pointer(control + (legacy ? 0x2b8 : 0x150) + shift, button); pointer(button + 8, 0x1f0000 + d); text(0x1f0000 + d, "MASTER");
            pointer(button + (legacy ? 0x2e0 : 0x1e0) + shift, value); masters[i] = value + 0x38; positions[i] = core + 0x20;
        }
        master(1);
    }
    ~RekordboxFixture() { VirtualFree(data_, 0, MEM_RELEASE); }
    RekordboxFixture(const RekordboxFixture &) = delete;
    RekordboxFixture &operator=(const RekordboxFixture &) = delete;
    std::uint64_t base() const { return reinterpret_cast<std::uint64_t>(data_); }
    bool read(std::uint64_t address, void *output, std::size_t count) override {
        if (hook) hook(address);
        if (address < base() || address - base() > size_ || count > size_ - (address - base())) return false;
        std::memcpy(output, data_ + (address - base()), count); return true;
    }
    template<class T> void put(std::size_t offset, T value) { std::memcpy(data_ + offset, &value, sizeof(value)); }
    void text(std::size_t offset, const std::string &value) { std::memset(data_ + offset, 0, value.size() + 1); std::memcpy(data_ + offset, value.data(), value.size()); }
    void master(unsigned deck) {
        for (unsigned i = 0; i < 4; ++i) {
            if (legacy_) { const auto variant = 0x200000 + (i + 1) * 0x1000; object(variant, i > 1 && deck != i + 1 ? "VariantType_Void@var@juce" : "VariantType_Bool@var@juce"); pointer(masters[i], variant); put<unsigned char>(masters[i] + 8, deck == i + 1 ? 1 : 0); }
            else put<std::uint64_t>(masters[i], deck == i + 1 ? 1 : 0);
        }
    }
private:
    unsigned char *data_ = nullptr; static constexpr std::size_t size_ = 0x400000; bool legacy_;
    std::map<std::string, std::size_t> names_; std::size_t nextType_ = 0x80000;
    void pointer(std::size_t offset, std::size_t value) { put<std::uint64_t>(offset, base() + value); }
    void object(std::size_t offset, const std::string &name) {
        auto found = names_.find(name);
        if (found == names_.end()) {
            const auto table = nextType_; nextType_ += 0x400; names_[name] = table;
            pointer(table - 8, table + 0x80); put<std::uint32_t>(table + 0x80, 1); put<std::uint32_t>(table + 0x8c, static_cast<std::uint32_t>(table + 0x100)); put<std::uint32_t>(table + 0x94, static_cast<std::uint32_t>(table + 0x80)); text(table + 0x110, ".?AV" + name + "@@");
        }
        pointer(offset, names_.at(name));
    }
};
