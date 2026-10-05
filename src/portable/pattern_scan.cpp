#include "pattern_scan.h"

#include <cstring>
#include <stdexcept>
#include <string>

namespace {

int HexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

}  // namespace

Pattern ParsePattern(std::string_view text) {
    Pattern pattern;
    size_t i = 0;
    while (i < text.size()) {
        if (text[i] == ' ') {
            ++i;
            continue;
        }
        if (i + 1 >= text.size() || (i + 2 < text.size() && text[i + 2] != ' ')) {
            throw std::invalid_argument("bad pattern token at " + std::to_string(i));
        }
        if (text[i] == '?' && text[i + 1] == '?') {
            pattern.push_back(std::nullopt);
        } else {
            const int high = HexDigit(text[i]);
            const int low = HexDigit(text[i + 1]);
            if (high < 0 || low < 0) {
                throw std::invalid_argument("bad pattern token at " + std::to_string(i));
            }
            pattern.push_back(static_cast<uint8_t>(high * 16 + low));
        }
        i += 2;
    }
    if (pattern.empty()) {
        throw std::invalid_argument("empty pattern");
    }
    return pattern;
}

std::vector<size_t> FindPattern(std::span<const uint8_t> bytes, const Pattern& pattern) {
    std::vector<size_t> matches;
    if (pattern.size() > bytes.size()) {
        return matches;
    }
    for (size_t start = 0; start + pattern.size() <= bytes.size(); ++start) {
        bool match = true;
        for (size_t i = 0; i < pattern.size(); ++i) {
            if (pattern[i] && *pattern[i] != bytes[start + i]) {
                match = false;
                break;
            }
        }
        if (match) {
            matches.push_back(start);
        }
    }
    return matches;
}

size_t ResolveRelativeCall(std::span<const uint8_t> bytes, size_t offset) {
    int32_t displacement = 0;
    std::memcpy(&displacement, bytes.data() + offset + 1, sizeof(displacement));
    return offset + 5 + displacement;
}
