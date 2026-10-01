#pragma once

#include <string>
#include <string_view>

#include <meojson/json.hpp>

#include "Common/TaskReason.hpp"

namespace asst::task_reason
{
inline json::value make_task_result(
    json::value basic_info,
    std::string_view status,
    std::string_view reason = {},
    json::object extra = {})
{
    basic_info["what"] = std::string(WhatTaskResult);
    if (!basic_info["details"].is_object()) {
        basic_info["details"] = json::object();
    }
    auto& details = basic_info["details"].as_object();
    details["status"] = std::string(status);
    if (!reason.empty()) {
        details["reason"] = std::string(reason);
    }
    for (auto& [key, value] : extra) {
        details.emplace(key, std::move(value));
    }
    return basic_info;
}
}
