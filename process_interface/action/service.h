#ifndef PROCESS_INTERFACE_ACTION_SERVICE_H
#define PROCESS_INTERFACE_ACTION_SERVICE_H

#include <string>

#include "../common/control_script_runner.h"

namespace ProcessInterface {
namespace Action {

bool ListActionsJson(
    const Common::ControlScriptRunner& runner,
    const std::string& app_id,
    std::string& response_json,
    std::string& error_message);

bool InvokeAction(
    const Common::ControlScriptRunner& runner,
    const std::string& app_id,
    const std::string& action_name,
    const std::string& args_json,
    double timeout_seconds,
    std::string& response_json,
    std::string& error_message);

}  // namespace Action
}  // namespace ProcessInterface

#endif  // PROCESS_INTERFACE_ACTION_SERVICE_H
