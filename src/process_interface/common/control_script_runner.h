#ifndef PROCESS_INTERFACE_COMMON_CONTROL_SCRIPT_RUNNER_H
#define PROCESS_INTERFACE_COMMON_CONTROL_SCRIPT_RUNNER_H

#include <map>
#include <string>
#include <vector>

#include "../../common/fs_compat.h"
#include "../../common/path_templates.h"

namespace ProcessInterface {
namespace Common {

class ControlScriptRunner {
public:
    ControlScriptRunner(const std::string& repo_root, const Common::PathTemplateSet& path_templates);

    bool RunConfigGet(const std::string& app_id, std::string& json_payload, std::string& error_message) const;
    bool RunConfigSet(
        const std::string& app_id,
        const std::string& key,
        const std::string& value,
        std::string& json_payload,
        std::string& error_message) const;
    bool RunActionList(const std::string& app_id, std::string& json_payload, std::string& error_message) const;
    bool RunActionInvoke(
        const std::string& app_id,
        const std::string& action_name,
        const std::string& args_json,
        double timeout_seconds,
        std::string& json_payload,
        std::string& error_message) const;
    bool RunActionJobGet(
        const std::string& app_id,
        const std::string& job_id,
        std::string& json_payload,
        std::string& error_message) const;

private:
    bool ParseArgsObject(
        const std::string& args_json,
        std::map<std::string, std::string>& args_out,
        std::string& error_message) const;

    fs::path repo_root_;
    Common::PathTemplateSet path_templates_;
};

ControlScriptRunner CreateControlScriptRunner(
    const std::string& repo_root,
    const Common::PathTemplateSet& path_templates);

}  // namespace Common
}  // namespace ProcessInterface

#endif  // PROCESS_INTERFACE_COMMON_CONTROL_SCRIPT_RUNNER_H
