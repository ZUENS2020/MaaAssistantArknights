#include "GameStatusImageAnalyzer.h"

#include "Config/TaskData.h"
#include "Utils/Logger.hpp"
#include "Utils/StringMisc.hpp"
#include "Vision/Matcher.h"
#include "Vision/RegionOCRer.h"

std::optional<asst::SlashCount>
    asst::GameStatusImageAnalyzer::analyze_slash(const cv::Mat& image, std::string_view task_name)
{
    RegionOCRer analyzer(image);
    analyzer.set_task_info(std::string(task_name));
    auto res_opt = analyzer.analyze();
    if (!res_opt) {
        Log.info(__FUNCTION__, "ocr miss", task_name);
        return std::nullopt;
    }
    auto parsed = parse_slash_count(res_opt->text);
    if (!parsed) {
        Log.info(__FUNCTION__, "slash parse failed", task_name, res_opt->text);
    }
    return parsed;
}

std::optional<int> asst::GameStatusImageAnalyzer::analyze_number(const cv::Mat& image, std::string_view task_name)
{
    RegionOCRer analyzer(image);
    analyzer.set_task_info(std::string(task_name));
    auto res_opt = analyzer.analyze();
    if (!res_opt) {
        return std::nullopt;
    }
    std::string text = res_opt->text;
    strip_ocr_noise(text);
    // Top-bar numbers sit right before a "+" button whose position moves with the digit count;
    // tolerate trailing non-digit noise from it ("0+" / "1305U").
    while (!text.empty() && (text.back() < '0' || text.back() > '9')) {
        text.pop_back();
    }
    int value = 0;
    if (text.empty() || !utils::chars_to_number(text, value) || value < 0) {
        Log.info(__FUNCTION__, "number parse failed", task_name, res_opt->text);
        return std::nullopt;
    }
    return value;
}

std::optional<std::string> asst::GameStatusImageAnalyzer::analyze_text(const cv::Mat& image, std::string_view task_name)
{
    RegionOCRer analyzer(image);
    analyzer.set_task_info(std::string(task_name));
    auto res_opt = analyzer.analyze();
    if (!res_opt || res_opt->text.empty()) {
        return std::nullopt;
    }
    return res_opt->text;
}

asst::HomeCurrencyResult asst::GameStatusImageAnalyzer::analyze_home_currency(const cv::Mat& image)
{
    HomeCurrencyResult result;
    result.originite = analyze_number(image, "Status-OriginiteOcr");
    result.orundum = analyze_number(image, "Status-OrundumOcr");
    result.lmd = analyze_number(image, "Status-LmdOcr");
    return result;
}

std::optional<asst::SlashCount> asst::GameStatusImageAnalyzer::analyze_home_sanity(const cv::Mat& image)
{
    auto current = analyze_number(image, "Status-SanityHomeCurrentOcr");
    std::optional<int> max;
    if (auto text = analyze_text(image, "Status-SanityHomeMaxOcr")) {
        max = parse_trailing_number(*text);
        if (!max) {
            Log.info(__FUNCTION__, "sanity max parse failed", *text);
        }
    }
    if (current && max && *max > 0 && *max < 1000 && *current >= 0 && *current < 1000) {
        return SlashCount { .current = *current, .max = *max };
    }
    Log.info(__FUNCTION__, "home sanity miss", current.value_or(-1), max.value_or(-1));
    return analyze_topbar_sanity(image);
}

std::optional<asst::SlashCount> asst::GameStatusImageAnalyzer::analyze_topbar_sanity(const cv::Mat& image)
{
    auto parsed = analyze_slash(image, "SanityMatch");
    if (parsed && parsed->max < 1000) {
        return parsed;
    }
    return std::nullopt;
}

std::optional<asst::SlashCount> asst::GameStatusImageAnalyzer::analyze_terminal_weekly(const cv::Mat& image)
{
    auto parsed = analyze_slash(image, "Status-AnnihilationWeeklyTerminal");
    if (!parsed) {
        return std::nullopt;
    }
    return pick_weekly_progress({ *parsed });
}

asst::AnnihilationStatusResult asst::GameStatusImageAnalyzer::analyze_annihilation(const cv::Mat& image)
{
    AnnihilationStatusResult result;

    Matcher full_record(image);
    full_record.set_task_info("UsePrts-AnnihilationCheck");
    if (full_record.analyze()) {
        result.record_full = true;
        result.can_agent = true;
    }

    Matcher full_record_on(image);
    full_record_on.set_task_info("UsePrts-AnnihilationSuccessCheck");
    if (full_record_on.analyze()) {
        result.record_full = true;
        result.can_agent = true;
    }

    Matcher regular_agent(image);
    regular_agent.set_task_info("UsePrtsSuccessCheck");
    if (regular_agent.analyze()) {
        result.can_agent = true;
    }

    Matcher unable(image);
    unable.set_task_info("Annihilation@UnableToAgent2");
    if (unable.analyze()) {
        result.unable_to_agent = true;
    }

    if (auto map_name = analyze_text(image, "Status-AnnihilationMapName")) {
        result.map_name = std::move(*map_name);
    }

    result.prts_cards = analyze_number(image, "Status-AnnihilationPrtsCards");

    std::vector<SlashCount> slash_hits;
    if (auto weekly = analyze_slash(image, "Status-AnnihilationWeekly")) {
        slash_hits.emplace_back(*weekly);
    }
    if (auto extra = analyze_slash(image, "Status-AnnihilationWeeklyAlt")) {
        slash_hits.emplace_back(*extra);
    }
    result.weekly = pick_weekly_progress(slash_hits);
    if (!result.weekly && slash_hits.size() == 1) {
        result.weekly = slash_hits.front();
    }

    Log.info(
        __FUNCTION__,
        "map",
        result.map_name,
        "record_full",
        result.record_full,
        "can_agent",
        result.can_agent,
        "unable",
        result.unable_to_agent,
        "cards",
        result.prts_cards.value_or(-1),
        "weekly",
        result.weekly ? result.weekly->current : -1,
        "/",
        result.weekly ? result.weekly->max : -1);

    return result;
}

std::optional<asst::SlashCount> asst::GameStatusImageAnalyzer::analyze_drones(const cv::Mat& image)
{
    return analyze_slash(image, "Status-DronesOcr");
}
