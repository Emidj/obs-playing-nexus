// SPDX-License-Identifier: MIT
#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>

namespace playing::providers::rekordbox {
// Bounded JSON recognizer for the audited ["db-path", "..."] entries.
// No regex path unescaping or permissive substring extraction.
class OptionsJson {
public:
    explicit OptionsJson(std::string_view input) : input_(input) {}
    std::vector<std::string> paths()
    {
        value(0); whitespace();
        if (position_ != input_.size()) fail();
        return std::move(paths_);
    }
private:
    std::string_view input_;
    std::size_t position_ = 0;
    std::vector<std::string> paths_;
    [[noreturn]] static void fail() { throw std::runtime_error("Invalid rekordbox options"); }
    void whitespace()
    { while (position_ < input_.size() && (input_[position_] == ' ' || input_[position_] == '\t'
        || input_[position_] == '\r' || input_[position_] == '\n')) ++position_; }
    bool take(char c)
    {
        whitespace();
        if (position_ < input_.size() && input_[position_] == c) { ++position_; return true; }
        return false;
    }
    unsigned hex4()
    {
        if (input_.size() - position_ < 4) fail();
        unsigned result = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = input_[position_++];
            unsigned digit;
            if (c >= '0' && c <= '9') digit = static_cast<unsigned>(c - '0');
            else if (c >= 'A' && c <= 'F') digit = static_cast<unsigned>(c - 'A' + 10);
            else if (c >= 'a' && c <= 'f') digit = static_cast<unsigned>(c - 'a' + 10);
            else { fail(); }
            result = result * 16 + digit;
        }
        return result;
    }
    static void codepoint(std::string &out, unsigned value)
    {
        if (value <= 0x7f) out += static_cast<char>(value);
        else if (value <= 0x7ff) {
            out += static_cast<char>(0xc0 | (value >> 6)); out += static_cast<char>(0x80 | (value & 63));
        } else if (value <= 0xffff) {
            out += static_cast<char>(0xe0 | (value >> 12)); out += static_cast<char>(0x80 | ((value >> 6) & 63));
            out += static_cast<char>(0x80 | (value & 63));
        } else {
            out += static_cast<char>(0xf0 | (value >> 18)); out += static_cast<char>(0x80 | ((value >> 12) & 63));
            out += static_cast<char>(0x80 | ((value >> 6) & 63)); out += static_cast<char>(0x80 | (value & 63));
        }
    }
    std::string string()
    {
        if (!take('"')) fail();
        std::string out;
        while (position_ < input_.size()) {
            char c = input_[position_++];
            if (c == '"') return out;
            if (static_cast<unsigned char>(c) < 32) fail();
            if (c != '\\') { out += c; continue; }
            if (position_ == input_.size()) fail();
            c = input_[position_++];
            switch (c) {
            case '"': case '\\': case '/': out += c; break;
            case 'b': out += '\b'; break; case 'f': out += '\f'; break;
            case 'n': out += '\n'; break; case 'r': out += '\r'; break; case 't': out += '\t'; break;
            case 'u': {
                unsigned point = hex4();
                if (point >= 0xd800 && point <= 0xdbff) {
                    if (input_.substr(position_, 2) != "\\u") fail();
                    position_ += 2; const auto low = hex4();
                    if (low < 0xdc00 || low > 0xdfff) fail();
                    point = 0x10000 + ((point - 0xd800) << 10) + low - 0xdc00;
                } else if (point >= 0xdc00 && point <= 0xdfff) fail();
                codepoint(out, point); break;
            }
            default: fail();
            }
        }
        fail();
    }
    struct Value { bool isString = false; std::string text; };
    Value value(unsigned depth)
    {
        if (depth > 32) fail();
        whitespace();
        if (position_ == input_.size()) fail();
        if (input_[position_] == '"') return {true, string()};
        if (take('[')) {
            std::vector<Value> first;
            std::size_t count = 0;
            if (!take(']')) {
                do { auto item = value(depth + 1); if (count++ < 2) first.push_back(std::move(item)); }
                while (take(','));
                if (!take(']')) fail();
            }
            if (count && first[0].isString && first[0].text == "db-path") {
                if (count != 2 || !first[1].isString || first[1].text.empty() || paths_.size() == 16) fail();
                paths_.push_back(std::move(first[1].text));
            }
            return {};
        }
        if (take('{')) {
            if (!take('}')) {
                do { string(); if (!take(':')) fail(); value(depth + 1); } while (take(','));
                if (!take('}')) fail();
            }
            return {};
        }
        for (const auto word : {std::string_view("true"), std::string_view("false"), std::string_view("null")}) {
            if (input_.substr(position_, word.size()) == word) { position_ += word.size(); return {}; }
        }
        // JSON number grammar; numeric values are not used as paths.
        if (input_[position_] == '-') ++position_;
        if (position_ < input_.size() && input_[position_] == '0') ++position_;
        else {
            if (position_ == input_.size() || input_[position_] < '1' || input_[position_] > '9') fail();
            while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') ++position_;
        }
        if (position_ < input_.size() && input_[position_] == '.') {
            ++position_; const auto start = position_;
            while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') ++position_;
            if (position_ == start) fail();
        }
        if (position_ < input_.size() && (input_[position_] == 'e' || input_[position_] == 'E')) {
            ++position_; if (position_ < input_.size() && (input_[position_] == '+' || input_[position_] == '-')) ++position_;
            const auto start = position_;
            while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') ++position_;
            if (position_ == start) fail();
        }
        return {};
    }
};
} // namespace playing::providers::rekordbox
