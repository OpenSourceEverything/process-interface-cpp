#ifndef GPI_CONTROL_SCRIPT_RUNNER_H
#define GPI_CONTROL_SCRIPT_RUNNER_H

#include <string>

namespace gpi {

class ControlScriptRunner {
public:
    ControlScriptRunner(
        const std::string& repo_root,
        const std::string& script_relative_path,
        const std::string& script_label);

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

private:
    bool RunSimpleCommand(
        const std::string& command_name,
        const std::string& app_id,
        const std::string& key,
        const std::string& value,
        std::string& json_payload,
        std::string& error_message) const;
    bool WriteArgsJsonFile(const std::string& args_json, std::string& path_out, std::string& error_message) const;

    std::string repo_root_;
    std::string script_relative_path_;
    std::string script_label_;
};

ControlScriptRunner CreateControlScriptRunner(const std::string& repo_root);

}  // namespace gpi

#endif  // GPI_CONTROL_SCRIPT_RUNNER_H
