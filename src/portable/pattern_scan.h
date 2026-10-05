#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

// A byte signature; nullopt entries are wildcards.
using Pattern = std::vector<std::optional<uint8_t>>;

// Parses "48 8B ?? E8" style text. Throws std::invalid_argument on malformed input.
Pattern ParsePattern(std::string_view text);

// Offsets of every place the pattern matches in bytes, in ascending order.
std::vector<size_t> FindPattern(std::span<const uint8_t> bytes, const Pattern& pattern);

// For an E8 rel32 call instruction at offset, returns the offset of the call target.
size_t ResolveRelativeCall(std::span<const uint8_t> bytes, size_t offset);
