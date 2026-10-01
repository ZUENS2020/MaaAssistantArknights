#pragma once

#include <charconv>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace asst
{
inline constexpr double DelayMultiplierDefault = 1.0;
inline constexpr double DelayMultiplierMin = 0.1;
inline constexpr double DelayMultiplierMax = 10.0;

// Scale a millisecond delay. 0 stays 0 so "no wait" is unchanged at any multiplier.
inline unsigned scale_delay_ms(unsigned millisecond, double multiplier) noexcept
{
    if (millisecond == 0 || multiplier == DelayMultiplierDefault) {
        return millisecond;
    }
    if (!std::isfinite(multiplier) || multiplier <= 0) {
        return millisecond;
    }
    const double scaled = static_cast<double>(millisecond) * multiplier;
    if (scaled >= static_cast<double>(UINT32_MAX)) {
        return UINT32_MAX;
    }
    return static_cast<unsigned>(scaled + 0.5);
}

inline int scale_timeout_seconds(int seconds, double multiplier) noexcept
{
    if (seconds <= 0 || multiplier == DelayMultiplierDefault) {
        return seconds;
    }
    if (!std::isfinite(multiplier) || multiplier <= 0) {
        return seconds;
    }
    const double scaled = static_cast<double>(seconds) * multiplier;
    if (scaled >= static_cast<double>(INT32_MAX)) {
        return INT32_MAX;
    }
    const int rounded = static_cast<int>(scaled + 0.5);
    return rounded < 1 ? 1 : rounded;
}

// Parse instance-option DelayMultiplier. Rejects non-finite / out-of-range values.
inline bool parse_delay_multiplier(std::string_view value, double& out) noexcept
{
    if (value.empty()) {
        return false;
    }
    double parsed = 0;
    const char* first = value.data();
    const char* last = first + value.size();
    const auto [ptr, ec] = std::from_chars(first, last, parsed);
    if (ec != std::errc {} || ptr != last || !std::isfinite(parsed)) {
        return false;
    }
    if (parsed < DelayMultiplierMin || parsed > DelayMultiplierMax) {
        return false;
    }
    out = parsed;
    return true;
}
}
