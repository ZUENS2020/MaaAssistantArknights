#include "InstHelper.h"

#include <algorithm>
#include <chrono>
#include <sstream>
#include <thread>

#include "Assistant.h"
#include "Common/DelayScaler.hpp"
#include "Utils/Logger.hpp"

asst::InstHelper::InstHelper(asst::Assistant* inst) :
    m_inst(inst)
{
}

std::shared_ptr<asst::Controller> asst::InstHelper::ctrler() const
{
    return m_inst ? m_inst->ctrler() : nullptr;
}

std::shared_ptr<asst::Status> asst::InstHelper::status() const
{
    return m_inst ? m_inst->status() : nullptr;
}

bool asst::InstHelper::need_exit() const
{
    return m_inst != nullptr && m_inst->need_exit();
}

bool asst::InstHelper::sleep(unsigned millisecond) const
{
    if (need_exit()) {
        return false;
    }
    if (millisecond == 0) {
        std::this_thread::yield();
        return true;
    }
    millisecond = scale_delay_ms(millisecond, delay_multiplier());
    Log.trace("ready to sleep", millisecond);
    auto millisecond_ms = std::chrono::milliseconds(millisecond);
    auto interval = std::chrono::milliseconds(std::min(millisecond, 5000U));

    for (auto sleep_time = interval; sleep_time <= millisecond_ms && !need_exit(); sleep_time += interval) {
        std::this_thread::sleep_for(interval);
    }
    if (!need_exit()) {
        std::this_thread::sleep_for(millisecond_ms % interval);
    }
    Log.trace("end of sleep", millisecond);

    return !need_exit();
}

double asst::InstHelper::delay_multiplier() const
{
    return m_inst ? m_inst->delay_multiplier() : DelayMultiplierDefault;
}

bool asst::InstHelper::save_failure_screenshot() const
{
    return m_inst ? m_inst->save_failure_screenshot() : true;
}

int asst::InstHelper::scaled_timeout_seconds(int seconds) const
{
    return scale_timeout_seconds(seconds, delay_multiplier());
}

asst::Assistant* asst::InstHelper::inst() noexcept
{
    return m_inst;
}

std::string asst::InstHelper::inst_string() const
{
    std::stringstream ss;
    ss << m_inst;
    return ss.str();
}
