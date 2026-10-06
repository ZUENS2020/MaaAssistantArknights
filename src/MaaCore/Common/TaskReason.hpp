#pragma once

#include <string>
#include <string_view>

namespace asst::task_reason
{
inline constexpr std::string_view NoPrtsCard = "NO_PRTS_CARD";
inline constexpr std::string_view NoFullRecord = "NO_FULL_RECORD";
inline constexpr std::string_view WeeklyCapReached = "WEEKLY_CAP_REACHED";
inline constexpr std::string_view AnnihilationNotFound = "ANNIHILATION_NOT_FOUND";
inline constexpr std::string_view RecognitionFailed = "RECOGNITION_FAILED";
inline constexpr std::string_view MaxCardsReached = "MAX_CARDS_REACHED";

inline constexpr std::string_view StatusSucceeded = "succeeded";
inline constexpr std::string_view StatusFailed = "failed";
inline constexpr std::string_view StatusSkipped = "skipped";

inline constexpr std::string_view WhatTaskResult = "TaskResult";
inline constexpr std::string_view WhatGameStatus = "GameStatus";
inline constexpr std::string_view WhatAnnihilationStatus = "AnnihilationStatus";
}

namespace asst
{
enum class OnNoCardAction
{
    Current, // keep historical Fight flow
    Skip,
    Fail,
    NormalDeploy,
};

enum class OnNoRecordAction
{
    Current, // keep historical Fight flow (UnableToAgent2 still fails)
    Skip,
    Fail,
};

inline bool parse_on_no_card(std::string_view value, OnNoCardAction& out)
{
    if (value.empty() || value == "current" || value == "default") {
        out = OnNoCardAction::Current;
        return true;
    }
    if (value == "skip") {
        out = OnNoCardAction::Skip;
        return true;
    }
    if (value == "fail") {
        out = OnNoCardAction::Fail;
        return true;
    }
    if (value == "normal_deploy") {
        out = OnNoCardAction::NormalDeploy;
        return true;
    }
    return false;
}

inline bool parse_on_no_record(std::string_view value, OnNoRecordAction& out)
{
    if (value.empty() || value == "current" || value == "default") {
        out = OnNoRecordAction::Current;
        return true;
    }
    if (value == "skip") {
        out = OnNoRecordAction::Skip;
        return true;
    }
    if (value == "fail") {
        out = OnNoRecordAction::Fail;
        return true;
    }
    return false;
}

enum class OnCapReachedAction
{
    Skip, // default: weekly orundum cap already reached -> do not fight Annihilation
    Current, // keep fighting (historical behaviour, report only)
    Fail,
};

inline bool parse_on_cap_reached(std::string_view value, OnCapReachedAction& out)
{
    if (value.empty() || value == "skip" || value == "default") {
        out = OnCapReachedAction::Skip;
        return true;
    }
    if (value == "current" || value == "ignore" || value == "continue") {
        out = OnCapReachedAction::Current;
        return true;
    }
    if (value == "fail") {
        out = OnCapReachedAction::Fail;
        return true;
    }
    return false;
}

inline bool is_annihilation_stage(std::string_view stage)
{
    return stage == "Annihilation" || stage.ends_with("@Annihilation");
}
}
