#include "bridge_adapter.h"

#include <string>

namespace gpi {

namespace {

bool IsSupportedApp(const std::string& app_id) {
    return app_id == "bridge";
}

}  // namespace

BridgeStatusAdapter::BridgeStatusAdapter(const std::string& bridge_repo_root)
    : bridge_repo_root_(bridge_repo_root),
      control_runner_(CreateControlScriptRunner(bridge_repo_root)) {}

bool BridgeStatusAdapter::ReadStatusJson(const std::string& app_id, std::string& status_json, std::string& error_message) {
    (void)status_json;
    if (!IsSupportedApp(app_id)) {
        error_message = "unsupported appId";
        return false;
    }
    error_message = "status.get is handled by ProcessInterface::Status in host runtime";
    return false;
}

bool BridgeStatusAdapter::GetConfigJson(const std::string& app_id, std::string& config_json, std::string& error_message) {
    if (!IsSupportedApp(app_id)) {
        error_message = "unsupported appId";
        return false;
    }
    return control_runner_.RunConfigGet(app_id, config_json, error_message);
}

bool BridgeStatusAdapter::SetConfigValue(
    const std::string& app_id,
    const std::string& key,
    const std::string& value,
    std::string& result_json,
    std::string& error_message) {
    if (!IsSupportedApp(app_id)) {
        error_message = "unsupported appId";
        return false;
    }
    return control_runner_.RunConfigSet(app_id, key, value, result_json, error_message);
}

bool BridgeStatusAdapter::ListActionsJson(const std::string& app_id, std::string& actions_json, std::string& error_message) {
    if (!IsSupportedApp(app_id)) {
        error_message = "unsupported appId";
        return false;
    }
    return control_runner_.RunActionList(app_id, actions_json, error_message);
}

bool BridgeStatusAdapter::InvokeAction(
    const std::string& app_id,
    const std::string& action_name,
    const std::string& args_json,
    double timeout_seconds,
    std::string& result_json,
    std::string& error_message) {
    if (!IsSupportedApp(app_id)) {
        error_message = "unsupported appId";
        return false;
    }
    return control_runner_.RunActionInvoke(app_id, action_name, args_json, timeout_seconds, result_json, error_message);
}

}  // namespace gpi
