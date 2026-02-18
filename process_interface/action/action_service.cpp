#include "service.h"

namespace ProcessInterface {
namespace Action {

bool ListActionsJson(
    const Common::ControlScriptRunner& runner,
    const std::string& app_id,
    std::string& response_json,
    std::string& error_message) {
    return runner.RunActionList(app_id, response_json, error_message);
}

bool InvokeAction(
    const Common::ControlScriptRunner& runner,
    const std::string& app_id,
    const std::string& action_name,
    const std::string& args_json,
    double timeout_seconds,
    std::string& response_json,
    std::string& error_message) {
    return runner.RunActionInvoke(app_id, action_name, args_json, timeout_seconds, response_json, error_message);
}

}  // namespace Action
}  // namespace ProcessInterface
