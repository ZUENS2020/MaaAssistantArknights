#include "InterfaceTask.h"

bool asst::InterfaceTask::run()
{
    if (PackageTask::run()) {
        return true;
    }

    if (save_failure_screenshot()) {
        save_img(utils::path("debug") / utils::path("interface"));
    }
    return false;
}
