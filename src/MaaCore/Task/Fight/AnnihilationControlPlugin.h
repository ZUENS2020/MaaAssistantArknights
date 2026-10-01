#pragma once

#include "Common/TaskReason.hpp"
#include "Task/AbstractTaskPlugin.h"

namespace asst
{
class ProcessTask;

// Counts finished annihilation battles and enforces max_cards / on_no_card=normal_deploy.
class AnnihilationControlPlugin final : public AbstractTaskPlugin
{
public:
    using AbstractTaskPlugin::AbstractTaskPlugin;
    virtual ~AnnihilationControlPlugin() override = default;

    virtual bool verify(AsstMsg msg, const json::value& details) const override;

    void set_max_cards(int max_cards) noexcept { m_max_cards = max_cards; }

    void set_on_no_card(OnNoCardAction action) noexcept { m_on_no_card = action; }

protected:
    virtual bool _run() override;

private:
    int m_max_cards = -1;
    OnNoCardAction m_on_no_card = OnNoCardAction::Current;
    mutable int m_cards_used = 0;
    mutable bool m_switched_to_normal = false;
};
}
