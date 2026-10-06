#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "Common/AsstTypes.h"
#include "Common/SlashCount.hpp"
#include "MaaUtils/NoWarningCVMat.hpp"

namespace asst
{
struct HomeCurrencyResult
{
    std::optional<int> orundum;
    std::optional<int> originite;
    std::optional<int> lmd;
};

struct AnnihilationStatusResult
{
    std::string map_name;
    std::optional<SlashCount> weekly;
    std::optional<int> prts_cards;
    bool record_full = false; // 全权委托 available (full 400-kill record)
    bool can_agent = false;   // regular 代理指挥 or 全权委托 available
    bool unable_to_agent = false;
};

class GameStatusImageAnalyzer
{
public:
    static std::optional<SlashCount> analyze_slash(const cv::Mat& image, std::string_view task_name);
    static std::optional<int> analyze_number(const cv::Mat& image, std::string_view task_name);
    static std::optional<std::string> analyze_text(const cv::Mat& image, std::string_view task_name);
    static HomeCurrencyResult analyze_home_currency(const cv::Mat& image);
    // Home screen: big "29" + "理智/205" label. Falls back to the stage-page top bar (SanityMatch).
    static std::optional<SlashCount> analyze_home_sanity(const cv::Mat& image);
    // Stage / annihilation prep page top bar "29/205".
    static std::optional<SlashCount> analyze_topbar_sanity(const cv::Mat& image);
    // Terminal To-Do list "0/1800" next to 合成玉.
    static std::optional<SlashCount> analyze_terminal_weekly(const cv::Mat& image);
    static AnnihilationStatusResult analyze_annihilation(const cv::Mat& image);
    static std::optional<SlashCount> analyze_drones(const cv::Mat& image);
};
}
