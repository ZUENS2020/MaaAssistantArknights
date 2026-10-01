#include "AnnihilationControlPlugin.h"

#include "Common/TaskResult.hpp"
#include "Task/ProcessTask.h"
#include "Utils/Logger.hpp"

bool asst::AnnihilationControlPlugin::verify(AsstMsg msg, const json::value& details) const
{
    if (m_max_cards < 0 && m_on_no_card != OnNoCardAction::NormalDeploy) {
        return false;
    }
    if (msg != AsstMsg::SubTaskStart || details.get("subtask", std::string()) != "ProcessTask") {
        return false;
    }

    const std::string task = details.get("details", "task", "");
    if (task.ends_with("EndOfActionAnnihilation")) {
        ++m_cards_used;
        Log.info(__FUNCTION__, "annihilation battles finished", m_cards_used);
        return m_max_cards >= 0 && m_cards_used >= m_max_cards;
    }
    return false;
}

bool asst::AnnihilationControlPlugin::_run()
{
    LogTraceFunction;

    if (m_max_cards < 0 || m_cards_used < m_max_cards) {
        return true;
    }

    auto* fight = dynamic_cast<ProcessTask*>(m_task_ptr);
    if (!fight) {
        return true;
    }

    if (m_on_no_card == OnNoCardAction::NormalDeploy) {
        if (!m_switched_to_normal) {
            Log.info(__FUNCTION__, "max_cards reached, switching to regular auto-deploy");
            fight->set_times_limit("UsePrts-Annihilation", 0);
            m_switched_to_normal = true;
        }
        return true;
    }

    Log.info(__FUNCTION__, "max_cards reached, stopping fight");
    fight->set_enable(false);
    callback(
        AsstMsg::SubTaskExtraInfo,
        task_reason::make_task_result(
            basic_info(),
            task_reason::StatusSucceeded,
            task_reason::MaxCardsReached,
            json::object { { "cards_used", m_cards_used } }));
    return true;
}
