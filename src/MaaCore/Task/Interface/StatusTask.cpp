#include "StatusTask.h"

#include <chrono>

#include "Common/TaskResult.hpp"
#include "Controller/Controller.h"
#include "Task/Fight/FightTimesTaskPlugin.h"
#include "Task/Miscellaneous/DepotRecognitionTask.h"
#include "Task/ProcessTask.h"
#include "Utils/Logger.hpp"
#include "Vision/Miscellaneous/GameStatusImageAnalyzer.h"

asst::StatusTask::StatusTask(const AsstCallback& callback, Assistant* inst) :
    InterfaceTask(callback, inst, TaskType)
{
    LogTraceFunction;
}

std::optional<asst::StatusTask::Params> asst::StatusTask::parse_params(const json::value& params)
{
    Params parsed;

    auto apply_field = [&](std::string_view field) -> bool {
        if (field == "sanity") {
            parsed.sanity = true;
            return true;
        }
        if (field == "currency") {
            parsed.currency = true;
            parsed.need_orundum = true;
            parsed.need_originite = true;
            return true;
        }
        if (field == "orundum") {
            parsed.currency = true;
            parsed.need_orundum = true;
            return true;
        }
        if (field == "originite") {
            parsed.currency = true;
            parsed.need_originite = true;
            return true;
        }
        if (field == "lmd") {
            parsed.currency = true;
            parsed.need_lmd = true;
            return true;
        }
        if (field == "annihilation") {
            parsed.annihilation = true;
            return true;
        }
        if (field == "depot") {
            parsed.depot = true;
            return true;
        }
        if (field == "drones") {
            parsed.drones = true;
            return true;
        }
        return false;
    };

    if (auto fields = params.find<json::array>("fields")) {
        if (fields->empty()) {
            Log.error(__FUNCTION__, "fields is empty");
            return std::nullopt;
        }
        for (const auto& item : *fields) {
            if (!item.is_string() || !apply_field(item.as_string())) {
                Log.error(__FUNCTION__, "unknown status field", item);
                return std::nullopt;
            }
        }
        return parsed;
    }

    parsed.sanity = params.get("sanity", false);
    const bool want_currency = params.get("currency", false);
    parsed.need_orundum = want_currency || params.get("orundum", false);
    parsed.need_originite = want_currency || params.get("originite", false);
    parsed.need_lmd = params.get("lmd", false);
    parsed.currency = parsed.need_orundum || parsed.need_originite || parsed.need_lmd;
    parsed.annihilation = params.get("annihilation", false);
    parsed.depot = params.get("depot", false);
    parsed.drones = params.get("drones", false);

    // Cheap default: only sanity, so a bare `type=Status` call does not walk the whole game.
    if (!parsed.any()) {
        parsed.sanity = true;
    }
    return parsed;
}

bool asst::StatusTask::set_params(const json::value& params)
{
    LogTraceFunction;

    auto parsed = parse_params(params);
    if (!parsed) {
        return false;
    }
    m_params = *parsed;
    Log.info(
        __FUNCTION__,
        "sanity",
        m_params.sanity,
        "currency",
        m_params.currency,
        "annihilation",
        m_params.annihilation,
        "depot",
        m_params.depot,
        "drones",
        m_params.drones);
    return true;
}

bool asst::StatusTask::run()
{
    LogTraceFunction;

    if (!m_enable) {
        Log.info("task disabled, pass", basic_info().to_string());
        return true;
    }

    json::object details;
    json::array errors;
    json::array warnings;
    bool ok = true;

    if (!go_home()) {
        Log.error(__FUNCTION__, "failed to return home");
        callback(
            AsstMsg::SubTaskExtraInfo,
            task_reason::make_task_result(basic_info(), task_reason::StatusFailed, task_reason::RecognitionFailed));
        return false;
    }

    if (m_params.sanity || m_params.currency) {
        recognize_home_fields(details, errors);
    }
    if (m_params.annihilation) {
        recognize_annihilation(details, errors, warnings);
        go_home();
    }
    if (m_params.drones) {
        if (!recognize_drones(details)) {
            errors.emplace_back("drones");
        }
        go_home();
    }
    if (m_params.depot) {
        if (!recognize_depot(details)) {
            errors.emplace_back("depot");
        }
        go_home();
    }

    if (!warnings.empty()) {
        details["warnings"] = std::move(warnings);
    }
    if (!errors.empty()) {
        details["errors"] = std::move(errors);
        ok = false;
    }

    emit_status(details);
    callback(
        AsstMsg::SubTaskExtraInfo,
        task_reason::make_task_result(
            basic_info(),
            ok ? task_reason::StatusSucceeded : task_reason::StatusFailed,
            ok ? std::string_view {} : task_reason::RecognitionFailed,
            json::object { { "partial", !ok } }));

    // Partial results are still a successful task chain so wrappers can read the JSON.
    return true;
}

bool asst::StatusTask::go_home()
{
    LogTraceFunction;
    ProcessTask task(*this, { "StatusBegin" });
    task.set_retry_times(5);
    return task.run();
}

namespace
{
json::object make_sanity_obj(const asst::SlashCount& sanity)
{
    json::object sanity_obj {
        { "current", sanity.current },
        { "max", sanity.max },
    };
    if (sanity.current < sanity.max) {
        const auto recover_s = static_cast<long long>(sanity.max - sanity.current) * 6 * 60; // 1 sanity / 6 min
        const auto next_full = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) + recover_s;
        sanity_obj["next_full_at"] = next_full;
    }
    return sanity_obj;
}

void remove_error(json::array& errors, std::string_view name)
{
    json::array kept;
    for (auto& item : errors) {
        if (!(item.is_string() && item.as_string() == name)) {
            kept.emplace_back(std::move(item));
        }
    }
    errors = std::move(kept);
}
}

void asst::StatusTask::recognize_home_fields(json::object& details, json::array& errors)
{
    LogTraceFunction;

    // Current CN UI: the home screen paints sanity as a big "29" plus a "理智/205" label, and the
    // top bar shows LMD / orundum / originite. (The terminal page no longer shows sanity.)
    auto image = ctrler()->get_image();

    if (m_params.sanity) {
        if (auto sanity = GameStatusImageAnalyzer::analyze_home_sanity(image)) {
            details["sanity"] = make_sanity_obj(*sanity);
        }
        else {
            Log.warn(__FUNCTION__, "sanity ocr failed on home screen");
            errors.emplace_back("sanity");
        }
    }

    if (m_params.currency) {
        auto currency = GameStatusImageAnalyzer::analyze_home_currency(image);
        auto put = [&](std::string_view key, const std::optional<int>& value, bool required) {
            if (value) {
                details[std::string(key)] = *value;
            }
            else if (required) {
                Log.warn(__FUNCTION__, "currency ocr failed", key);
                errors.emplace_back(std::string(key));
            }
        };
        put("orundum", currency.orundum, m_params.need_orundum);
        put("originite", currency.originite, m_params.need_originite);
        put("lmd", currency.lmd, m_params.need_lmd);
    }
}

void asst::StatusTask::recognize_annihilation(json::object& details, json::array& errors, json::array& warnings)
{
    LogTraceFunction;

    // The terminal To-Do list also shows the weekly orundum progress; read it on the way as a fallback.
    std::optional<SlashCount> terminal_weekly;
    {
        ProcessTask enter_terminal(*this, { "StatusEnterTerminal" });
        enter_terminal.set_retry_times(5);
        if (enter_terminal.run()) {
            terminal_weekly = GameStatusImageAnalyzer::analyze_terminal_weekly(ctrler()->get_image());
        }
        else {
            Log.warn(__FUNCTION__, "failed to enter terminal before annihilation");
        }
    }

    ProcessTask nav(*this, { "StatusAnnihilationBegin" });
    nav.set_retry_times(5);
    if (!nav.run()) {
        Log.warn(__FUNCTION__, "failed to open annihilation page");
        json::object anni;
        if (terminal_weekly) {
            anni["weekly_progress"] = terminal_weekly->current;
            anni["weekly_cap"] = terminal_weekly->max;
            details["annihilation"] = std::move(anni);
        }
        errors.emplace_back("annihilation");
        return;
    }

    auto image = ctrler()->get_image();
    auto info = GameStatusImageAnalyzer::analyze_annihilation(image);
    if (!info.weekly && terminal_weekly) {
        Log.info(__FUNCTION__, "using terminal To-Do weekly progress");
        info.weekly = terminal_weekly;
    }

    json::object anni;
    if (!info.map_name.empty()) {
        anni["map_name"] = info.map_name;
    }
    else {
        warnings.emplace_back("annihilation.map_name");
    }
    if (info.weekly) {
        anni["weekly_progress"] = info.weekly->current;
        anni["weekly_cap"] = info.weekly->max;
        anni["weekly_cap_reached"] = info.weekly->current >= info.weekly->max;
    }
    else {
        errors.emplace_back("annihilation.weekly_progress");
    }
    anni["record_full"] = info.record_full;
    anni["can_agent"] = info.can_agent;
    if (info.prts_cards) {
        anni["prts_cards"] = *info.prts_cards;
    }
    else {
        errors.emplace_back("annihilation.prts_cards");
    }
    details["annihilation"] = std::move(anni);

    // The prep page shows sanity in its top bar: use it if the home screen read failed.
    if (m_params.sanity && !details.contains("sanity")) {
        if (auto sanity = GameStatusImageAnalyzer::analyze_topbar_sanity(image)) {
            Log.info(__FUNCTION__, "sanity recovered from annihilation page top bar");
            details["sanity"] = make_sanity_obj(*sanity);
            remove_error(errors, "sanity");
        }
    }
}

bool asst::StatusTask::recognize_drones(json::object& details)
{
    LogTraceFunction;

    ProcessTask nav(*this, { "StatusInfrastBegin" });
    nav.set_retry_times(5);
    if (!nav.run()) {
        Log.warn(__FUNCTION__, "failed to open infrastructure");
        return false;
    }

    auto drones = GameStatusImageAnalyzer::analyze_drones(ctrler()->get_image());
    if (!drones) {
        Log.warn(__FUNCTION__, "drone ocr failed");
        return false;
    }
    details["drones"] = json::object {
        { "current", drones->current },
        { "max", drones->max },
    };
    return true;
}

bool asst::StatusTask::recognize_depot(json::object& details)
{
    LogTraceFunction;

    ProcessTask enter(*this, { "DepotBegin" });
    enter.set_ignore_error(true);
    enter.set_retry_times(5);
    if (!enter.run()) {
        Log.warn(__FUNCTION__, "failed to open depot");
        return false;
    }

    DepotRecognitionTask recognition(m_callback, m_inst, TaskType);
    recognition.set_task_id(m_task_id);
    recognition.set_retry_times(0);
    if (!recognition.run()) {
        Log.warn(__FUNCTION__, "depot recognition failed");
        return false;
    }
    json::object depot_obj;
    for (const auto& [item_id, item_info] : recognition.get_items()) {
        depot_obj.emplace(item_id, item_info.quantity);
    }
    details["depot"] = std::move(depot_obj);
    return true;
}

void asst::StatusTask::emit_status(const json::object& details)
{
    json::value info = basic_info_with_what(std::string(task_reason::WhatGameStatus));
    info["details"] = details;
    callback(AsstMsg::SubTaskExtraInfo, info);
}
