#pragma once

#include "Task/InterfaceTask.h"

#include <optional>
#include <string>
#include <vector>

namespace asst
{
class ProcessTask;
class DepotRecognitionTask;

class StatusTask final : public InterfaceTask
{
public:
    inline static constexpr std::string_view TaskType = "Status";

    StatusTask(const AsstCallback& callback, Assistant* inst);
    virtual ~StatusTask() override = default;

    virtual bool run() override;
    virtual bool set_params(const json::value& params) override;

    struct Params
    {
        bool sanity = false;
        bool currency = false;
        bool annihilation = false;
        bool depot = false;
        bool drones = false;
        // Which currency sub-fields must be present (missing ones go to `errors`).
        bool need_orundum = false;
        bool need_originite = false;
        bool need_lmd = false;

        bool any() const noexcept { return sanity || currency || annihilation || depot || drones; }
    };

    static std::optional<Params> parse_params(const json::value& params);

private:
    bool go_home();
    void recognize_home_fields(json::object& details, json::array& errors);
    void recognize_annihilation(json::object& details, json::array& errors, json::array& warnings);
    bool recognize_drones(json::object& details);
    bool recognize_depot(json::object& details);
    void emit_status(const json::object& details);

    Params m_params;
};
}
