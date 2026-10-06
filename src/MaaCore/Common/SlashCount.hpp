#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Utils/StringMisc.hpp"

namespace asst
{
struct SlashCount
{
    int current = 0;
    int max = 0;
};

inline void strip_ocr_noise(std::string& text)
{
    std::string cleaned;
    cleaned.reserve(text.size());
    for (char ch : text) {
        if (ch == ',' || ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
            continue;
        }
        cleaned.push_back(ch);
    }
    text.swap(cleaned);
}

// Parse "123/456" (optional spaces/commas). Used by Status / Annihilation precheck.
inline std::optional<SlashCount> parse_slash_count(std::string_view text)
{
    std::string cleaned(text);
    strip_ocr_noise(cleaned);
    const auto slash_pos = cleaned.find('/');
    if (slash_pos == std::string::npos || slash_pos == 0 || slash_pos + 1 >= cleaned.size()) {
        return std::nullopt;
    }

    int current = 0;
    int max = 0;
    if (!utils::chars_to_number(std::string_view(cleaned).substr(0, slash_pos), current) ||
        !utils::chars_to_number(std::string_view(cleaned).substr(slash_pos + 1), max)) {
        return std::nullopt;
    }
    if (current < 0 || max <= 0 || current > max * 2) {
        return std::nullopt;
    }
    return SlashCount { .current = current, .max = max };
}

// Parse the number after the last '/' (e.g. "理智/205" or "/205" -> 205); falls back to the
// whole string when there is no slash. Used for the home-screen sanity max label.
inline std::optional<int> parse_trailing_number(std::string_view text)
{
    std::string cleaned(text);
    strip_ocr_noise(cleaned);
    const auto slash_pos = cleaned.rfind('/');
    std::string_view digits = cleaned;
    if (slash_pos != std::string::npos) {
        digits = std::string_view(cleaned).substr(slash_pos + 1);
    }
    if (digits.empty()) {
        return std::nullopt;
    }
    int value = 0;
    if (!utils::chars_to_number(digits, value) || value < 0) {
        return std::nullopt;
    }
    return value;
}

inline std::optional<SlashCount> pick_weekly_progress(const std::vector<SlashCount>& hits)
{
    for (const auto& hit : hits) {
        if (hit.max >= 1000 && hit.max <= 2000) {
            return hit;
        }
    }
    return std::nullopt;
}
}
