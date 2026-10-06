#include "AnnihilationPrecheckTask.h"

#include "Common/TaskResult.hpp"
#include "Controller/Controller.h"
#include "Task/ProcessTask.h"
#include "Utils/Logger.hpp"

bool asst::AnnihilationPrecheckTask::_run()
{
    LogTraceFunction;

    if (m_fight_task_ptr && !m_fight_task_ptr->get_enable()) {
        Log.info(__FUNCTION__, "fight already disabled, treat as annihilation skip");
        callback(
            AsstMsg::SubTaskExtraInfo,
            task_reason::make_task_result(basic_info(), task_reason::StatusSkipped, task_reason::WeeklyCapReached));
        return true;
    }

    m_last_status = GameStatusImageAnalyzer::analyze_annihilation(ctrler()->get_image());
    m_overridden = false;
    if (m_override_prts_cards) {
        Log.warn(__FUNCTION__, "TEST OVERRIDE prts_cards", m_last_status.prts_cards.value_or(-1), "->", *m_override_prts_cards);
        m_last_status.prts_cards = *m_override_prts_cards;
        m_overridden = true;
    }
    if (m_override_weekly_progress) {
        const int cap = m_last_status.weekly ? m_last_status.weekly->max : 1800;
        Log.warn(
            __FUNCTION__,
            "TEST OVERRIDE weekly_progress",
            m_last_status.weekly ? m_last_status.weekly->current : -1,
            "->",
            *m_override_weekly_progress,
            "/",
            cap);
        m_last_status.weekly = SlashCount { .current = *m_override_weekly_progress, .max = cap };
        m_overridden = true;
    }
    emit_status();
    return apply_policies();
}

void asst::AnnihilationPrecheckTask::emit_status()
{
    json::value info = basic_info_with_what(std::string(task_reason::WhatAnnihilationStatus));
    auto& details = info["details"].as_object();
    if (!m_last_status.map_name.empty()) {
        details["map_name"] = m_last_status.map_name;
    }
    if (m_last_status.weekly) {
        details["weekly_progress"] = m_last_status.weekly->current;
        details["weekly_cap"] = m_last_status.weekly->max;
    }
    details["record_full"] = m_last_status.record_full;
    details["can_agent"] = m_last_status.can_agent;
    details["unable_to_agent"] = m_last_status.unable_to_agent;
    if (m_last_status.prts_cards) {
        details["prts_cards"] = *m_last_status.prts_cards;
    }
    if (m_last_status.weekly) {
        details["weekly_cap_reached"] = m_last_status.weekly->max > 0 &&
                                        m_last_status.weekly->current >= m_last_status.weekly->max;
    }
    if (m_overridden) {
        details["overridden"] = true;
    }
    callback(AsstMsg::SubTaskExtraInfo, info);
}

bool asst::AnnihilationPrecheckTask::apply_policies()
{
    const bool weekly_full = m_last_status.weekly && m_last_status.weekly->max > 0 &&
                             m_last_status.weekly->current >= m_last_status.weekly->max;

    if (weekly_full) {
        switch (m_on_cap_reached) {
        case OnCapReachedAction::Skip:
            Log.info(__FUNCTION__, "weekly cap already reached, skipping annihilation");
            if (m_fight_task_ptr) {
                m_fight_task_ptr->set_enable(false);
            }
            callback(
                AsstMsg::SubTaskExtraInfo,
                task_reason::make_task_result(basic_info(), task_reason::StatusSkipped, task_reason::WeeklyCapReached));
            return true;
        case OnCapReachedAction::Fail:
            Log.info(__FUNCTION__, "weekly cap already reached, failing as requested");
            callback(
                AsstMsg::SubTaskExtraInfo,
                task_reason::make_task_result(basic_info(), task_reason::StatusFailed, task_reason::WeeklyCapReached));
            return false;
        case OnCapReachedAction::Current:
            Log.info(__FUNCTION__, "weekly cap already reached, continuing (on_cap_reached=current)");
            break;
        }
    }
    if (!m_last_status.weekly) {
        Log.warn(__FUNCTION__, "weekly progress unreadable, cap check skipped");
    }
    if (!m_last_status.prts_cards) {
        Log.warn(__FUNCTION__, "PRTS card count unreadable, no-card policy not applied");
    }

    const bool no_record = m_last_status.unable_to_agent || !m_last_status.record_full;
    if (m_on_no_record == OnNoRecordAction::Fail && no_record) {
        callback(
            AsstMsg::SubTaskExtraInfo,
            task_reason::make_task_result(basic_info(), task_reason::StatusFailed, task_reason::NoFullRecord));
        return false;
    }
    if (m_on_no_record == OnNoRecordAction::Skip && no_record) {
        if (m_fight_task_ptr) {
            m_fight_task_ptr->set_enable(false);
        }
        callback(
            AsstMsg::SubTaskExtraInfo,
            task_reason::make_task_result(basic_info(), task_reason::StatusSkipped, task_reason::NoFullRecord));
        return true;
    }
    if (m_on_no_record == OnNoRecordAction::Current && m_last_status.unable_to_agent) {
        callback(
            AsstMsg::SubTaskExtraInfo,
            task_reason::make_task_result(basic_info(), task_reason::StatusFailed, task_reason::NoFullRecord));
        return false;
    }

    const bool no_card = m_last_status.prts_cards && *m_last_status.prts_cards <= 0;
    if (no_card) {
        if (m_on_no_card == OnNoCardAction::Fail) {
            callback(
                AsstMsg::SubTaskExtraInfo,
                task_reason::make_task_result(basic_info(), task_reason::StatusFailed, task_reason::NoPrtsCard));
            return false;
        }
        if (m_on_no_card == OnNoCardAction::Skip) {
            if (m_fight_task_ptr) {
                m_fight_task_ptr->set_enable(false);
            }
            callback(
                AsstMsg::SubTaskExtraInfo,
                task_reason::make_task_result(basic_info(), task_reason::StatusSkipped, task_reason::NoPrtsCard));
            return true;
        }
        if (m_on_no_card == OnNoCardAction::NormalDeploy && m_fight_task_ptr) {
            Log.info(__FUNCTION__, "no PRTS card, falling back to regular auto-deploy");
            m_fight_task_ptr->set_times_limit("UsePrts-Annihilation", 0);
        }
    }

    if (m_max_cards >= 0 && m_fight_task_ptr && m_on_no_card != OnNoCardAction::NormalDeploy) {
        m_fight_task_ptr->set_times_limit("StartButton1", m_max_cards);
        m_fight_task_ptr->set_times_limit("StartButton2", m_max_cards);
    }

    return true;
}
