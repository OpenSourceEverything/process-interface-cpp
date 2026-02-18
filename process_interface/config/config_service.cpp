#include "service.h"

namespace ProcessInterface {
namespace Config {

bool GetConfigJson(
    const Common::ControlScriptRunner& runner,
    const std::string& app_id,
    std::string& response_json,
    std::string& error_message) {
    return runner.RunConfigGet(app_id, response_json, error_message);
}

bool SetConfigValue(
    const Common::ControlScriptRunner& runner,
    const std::string& app_id,
    const std::string& key,
    const std::string& value,
    std::string& response_json,
    std::string& error_message) {
    return runner.RunConfigSet(app_id, key, value, response_json, error_message);
}

}  // namespace Config
}  // namespace ProcessInterface
