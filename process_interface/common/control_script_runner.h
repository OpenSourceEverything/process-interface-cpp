#ifndef PROCESS_INTERFACE_COMMON_CONTROL_SCRIPT_RUNNER_H
#define PROCESS_INTERFACE_COMMON_CONTROL_SCRIPT_RUNNER_H

#include <map>
#include <string>
#include <vector>

namespace ProcessInterface {
namespace Common {

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
    struct ActionDefinition {
        std::string name;
        std::string label;
        std::vector<std::string> command;
        std::string cwd;
        double timeout_seconds;
        bool detached;
        std::string args_json;
    };

    struct ActionRunResult {
        bool ok;
        int rc;
        bool detached;
        int pid;
        std::string payload_json;
        std::string stdout_text;
        std::string stderr_text;
        std::string error_code;
        std::string error_message;
    };

    bool LoadActionCatalog(
        const std::string& app_id,
        std::vector<ActionDefinition>& actions_out,
        std::string& error_message) const;

    bool ParseArgsObject(
        const std::string& args_json,
        std::map<std::string, std::string>& args_out,
        std::string& error_message) const;

    bool RunCatalogAction(
        const std::string& app_id,
        const std::string& action_name,
        const std::map<std::string, std::string>& args_map,
        double timeout_override_seconds,
        ActionRunResult& result_out,
        std::string& error_message) const;

    std::string BuildActionInvokeResponse(const ActionRunResult& result) const;
    static bool IsJsonObjectText(const std::string& text);
    static bool TryExtractFirstJsonObject(const std::string& text, std::string& object_json_out);
    static std::vector<std::string> SplitOutputLines(const std::string& text);
    static std::string BuildConfigSetFallbackPayload(
        const std::string& key,
        const std::string& value,
        int rc,
        const std::string& stdout_text);

    std::string repo_root_;
    std::string script_relative_path_;
    std::string script_label_;
};

ControlScriptRunner CreateControlScriptRunner(const std::string& repo_root);

}  // namespace Common
}  // namespace ProcessInterface

#endif  // PROCESS_INTERFACE_COMMON_CONTROL_SCRIPT_RUNNER_H
