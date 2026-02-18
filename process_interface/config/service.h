#ifndef PROCESS_INTERFACE_CONFIG_SERVICE_H
#define PROCESS_INTERFACE_CONFIG_SERVICE_H

#include <string>

#include "../common/control_script_runner.h"

namespace ProcessInterface {
namespace Config {

bool GetConfigJson(
    const Common::ControlScriptRunner& runner,
    const std::string& app_id,
    std::string& response_json,
    std::string& error_message);

bool SetConfigValue(
    const Common::ControlScriptRunner& runner,
    const std::string& app_id,
    const std::string& key,
    const std::string& value,
    std::string& response_json,
    std::string& error_message);

}  // namespace Config
}  // namespace ProcessInterface

#endif  // PROCESS_INTERFACE_CONFIG_SERVICE_H
