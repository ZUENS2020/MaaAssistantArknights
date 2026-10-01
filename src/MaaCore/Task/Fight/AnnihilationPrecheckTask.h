#pragma once

#include "Common/TaskReason.hpp"
#include "Task/AbstractTask.h"
#include "Vision/Miscellaneous/GameStatusImageAnalyzer.h"

namespace asst
{
class ProcessTask;

class AnnihilationPrecheckTask final : public AbstractTask
{
public:
    using AbstractTask::AbstractTask;
    virtual ~AnnihilationPrecheckTask() override = default;

    void set_fight_task_ptr(std::shared_ptr<ProcessTask> fight_task_ptr) noexcept
    {
        m_fight_task_ptr = std::move(fight_task_ptr);
    }

    void set_on_no_card(OnNoCardAction action) noexcept { m_on_no_card = action; }

    void set_on_no_record(OnNoRecordAction action) noexcept { m_on_no_record = action; }

    void set_max_cards(int max_cards) noexcept { m_max_cards = max_cards; }

    const AnnihilationStatusResult& last_status() const noexcept { return m_last_status; }

protected:
    virtual bool _run() override;

private:
    void emit_status();
    bool apply_policies();

    std::shared_ptr<ProcessTask> m_fight_task_ptr = nullptr;
    OnNoCardAction m_on_no_card = OnNoCardAction::Current;
    OnNoRecordAction m_on_no_record = OnNoRecordAction::Current;
    int m_max_cards = -1;
    AnnihilationStatusResult m_last_status;
};
}
