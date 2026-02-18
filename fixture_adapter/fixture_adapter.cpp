#include "fixture_adapter.h"

#include <string>

namespace gpi {

namespace {

bool IsSupportedApp(const std::string& app_id) {
    return app_id == "40318" || app_id == "plc-simulator" || app_id == "ble-simulator";
}

}  // namespace

FixtureStatusAdapter::FixtureStatusAdapter(const std::string& fixture_repo_root)
    : fixture_repo_root_(fixture_repo_root),
      control_runner_(CreateControlScriptRunner(fixture_repo_root)) {}

bool FixtureStatusAdapter::ReadStatusJson(const std::string& app_id, std::string& status_json, std::string& error_message) {
    (void)status_json;
    if (!IsSupportedApp(app_id)) {
        error_message = "unsupported appId";
        return false;
    }
    error_message = "status.get is handled by ProcessInterface::Status in host runtime";
    return false;
}

bool FixtureStatusAdapter::GetConfigJson(const std::string& app_id, std::string& config_json, std::string& error_message) {
    if (!IsSupportedApp(app_id)) {
        error_message = "unsupported appId";
        return false;
    }
    return control_runner_.RunConfigGet(app_id, config_json, error_message);
}

bool FixtureStatusAdapter::SetConfigValue(
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

bool FixtureStatusAdapter::ListActionsJson(const std::string& app_id, std::string& actions_json, std::string& error_message) {
    if (!IsSupportedApp(app_id)) {
        error_message = "unsupported appId";
        return false;
    }
    return control_runner_.RunActionList(app_id, actions_json, error_message);
}

bool FixtureStatusAdapter::InvokeAction(
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
