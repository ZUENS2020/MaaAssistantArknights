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
        if (field == "currency" || field == "orundum" || field == "originite" || field == "lmd") {
            parsed.currency = true;
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
    parsed.currency = params.get("currency", false) || params.get("orundum", false) || params.get("originite", false) ||
                      params.get("lmd", false);
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
    bool ok = true;

    if (!go_home()) {
        Log.error(__FUNCTION__, "failed to return home");
        callback(
            AsstMsg::SubTaskExtraInfo,
            task_reason::make_task_result(basic_info(), task_reason::StatusFailed, task_reason::RecognitionFailed));
        return false;
    }

    if (m_params.sanity || m_params.currency) {
        if (!recognize_home_fields(details)) {
            errors.emplace_back(m_params.sanity ? "sanity" : "currency");
        }
    }
    if (m_params.annihilation) {
        if (!recognize_annihilation(details)) {
            errors.emplace_back("annihilation");
        }
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

bool asst::StatusTask::recognize_home_fields(json::object& details)
{
    LogTraceFunction;

    // Terminal (and the stage-select top bar) is where sanity is painted as current/max.
    if (m_params.sanity) {
        ProcessTask enter_terminal(*this, { "StatusEnterTerminal" });
        enter_terminal.set_retry_times(5);
        if (!enter_terminal.run()) {
            Log.warn(__FUNCTION__, "failed to enter terminal for sanity");
        }
    }

    auto image = ctrler()->get_image();
    if (m_params.sanity) {
        if (auto sanity = FightTimesTaskPlugin::analyze_sanity_remain(image)) {
            json::object sanity_obj {
                { "current", sanity->current },
                { "max", sanity->max },
            };
            if (sanity->current < sanity->max) {
                const auto recover_s =
                    static_cast<long long>(sanity->max - sanity->current) * 6 * 60; // 1 sanity / 6 min
                const auto next_full =
                    std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) + recover_s;
                sanity_obj["next_full_at"] = next_full;
            }
            details["sanity"] = std::move(sanity_obj);
        }
        else {
            Log.warn(__FUNCTION__, "sanity ocr failed");
            return false;
        }
    }

    if (m_params.currency) {
        auto currency = GameStatusImageAnalyzer::analyze_home_currency(image);
        if (currency.orundum) {
            details["orundum"] = *currency.orundum;
        }
        if (currency.originite) {
            details["originite"] = *currency.originite;
        }
        if (currency.lmd) {
            details["lmd"] = *currency.lmd;
        }
        if (!currency.orundum && !currency.originite && !currency.lmd) {
            Log.warn(__FUNCTION__, "currency ocr failed");
            return false;
        }
    }
    return true;
}

bool asst::StatusTask::recognize_annihilation(json::object& details)
{
    LogTraceFunction;

    ProcessTask nav(*this, { "StatusAnnihilationBegin" });
    nav.set_retry_times(5);
    if (!nav.run()) {
        Log.warn(__FUNCTION__, "failed to open annihilation page");
        return false;
    }

    auto image = ctrler()->get_image();
    auto info = GameStatusImageAnalyzer::analyze_annihilation(image);
    json::object anni;
    if (!info.map_name.empty()) {
        anni["map_name"] = info.map_name;
    }
    if (info.weekly) {
        anni["weekly_progress"] = info.weekly->current;
        anni["weekly_cap"] = info.weekly->max;
    }
    anni["record_full"] = info.record_full;
    if (info.prts_cards) {
        anni["prts_cards"] = *info.prts_cards;
    }
    details["annihilation"] = std::move(anni);
    return true;
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
