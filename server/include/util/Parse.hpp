#pragma once

#include <charconv>
#include <optional>
#include <string_view>

namespace zappy {

// Whole-string integer parsing: "42" -> 42, "42x", "", "abc" -> nothing.
// Replaces atoi/stoi, which silently return 0 or throw.
inline std::optional<long> parseInt(std::string_view text)
{
    long value = 0;
    const char *end = text.data() + text.size();
    auto [ptr, ec] = std::from_chars(text.data(), end, value);
    if (text.empty() || ec != std::errc() || ptr != end)
        return std::nullopt;
    return value;
}

} // namespace zappy
