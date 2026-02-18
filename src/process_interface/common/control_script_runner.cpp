#include "control_script_runner.h"

#include "../../../external/nlohmann/json.hpp"
#include "../../common/time_utils.h"
#include "action_catalog.h"
#include "action_executor.h"
#include "action_jobs.h"
#include "action_response.h"

namespace ProcessInterface {
namespace Common {

namespace {

bool IsObjectJsonText(const std::string& text) {
    try {
        const nlohmann::json value = nlohmann::json::parse(text);
        return value.is_object();
    } catch (const std::exception&) {
        return false;
    }
}

std::string CompactObjectJsonOrDefault(const std::string& text) {
    try {
        const nlohmann::json value = nlohmann::json::parse(text);
        if (value.is_object()) {
            return value.dump();
        }
    } catch (const std::exception&) {
    }
    return std::string("{}");
}

}  // namespace

ControlScriptRunner::ControlScriptRunner(
    const std::string& repo_root,
    const Common::PathTemplateSet& path_templates)
    : repo_root_(repo_root),
      path_templates_(path_templates) {}

bool ControlScriptRunner::ParseArgsObject(
    const std::string& args_json,
    std::map<std::string, std::string>& args_out,
    std::string& error_message) const {
    args_out.clear();

    nlohmann::json args;
    try {
        args = nlohmann::json::parse(args_json.empty() ? "{}" : args_json);
    } catch (const std::exception&) {
        error_message = "args json must decode to an object";
        return false;
    }

    if (!args.is_object()) {
        error_message = "args json must decode to an object";
        return false;
    }

    nlohmann::json::const_iterator iter;
    for (iter = args.begin(); iter != args.end(); ++iter) {
        const std::string key = iter.key();
        const nlohmann::json& value = iter.value();

        if (value.is_string()) {
            args_out[key] = value.get<std::string>();
        } else if (value.is_null()) {
            args_out[key] = std::string();
        } else {
            args_out[key] = value.dump();
        }
    }

    return true;
}

bool ControlScriptRunner::RunConfigGet(
    const std::string& app_id,
    std::string& json_payload,
    std::string& error_message) const {
    std::vector<ActionDefinition> actions;
    if (!LoadActionCatalog(repo_root_, path_templates_, app_id, actions, error_message)) {
        return false;
    }

    ActionRunResult action_result;
    std::map<std::string, std::string> args;
    if (!ExecuteCatalogAction(repo_root_, actions, "config_show", args, 0.0, action_result, error_message)) {
        return false;
    }

    auto build_fallback = [&](const std::string& reason) {
        nlohmann::json fallback;
        fallback["repoRoot"] = repo_root_.string();
        fallback["valid"] = false;
        fallback["errors"] = nlohmann::json::array({reason});
        fallback["entries"] = nlohmann::json::object();
        fallback["paths"] = nlohmann::json::object();
        fallback["configTree"] = nlohmann::json::object();
        return fallback.dump();
    };

    if (!action_result.ok) {
        json_payload = build_fallback(action_result.error_message.empty() ? "config.get failed" : action_result.error_message);
        error_message.clear();
        return true;
    }

    if (!IsObjectJsonText(action_result.payload_json)) {
        json_payload = build_fallback("config.get returned non-JSON payload");
        error_message.clear();
        return true;
    }

    json_payload = CompactObjectJsonOrDefault(action_result.payload_json);
    return true;
}

bool ControlScriptRunner::RunConfigSet(
    const std::string& app_id,
    const std::string& key,
    const std::string& value,
    std::string& json_payload,
    std::string& error_message) const {
    std::vector<ActionDefinition> actions;
    if (!LoadActionCatalog(repo_root_, path_templates_, app_id, actions, error_message)) {
        return false;
    }

    ActionRunResult action_result;
    std::map<std::string, std::string> args;
    args["key"] = key;
    args["value"] = value;

    if (!ExecuteCatalogAction(repo_root_, actions, "config_set_key", args, 0.0, action_result, error_message)) {
        return false;
    }

    if (!action_result.ok) {
        json_payload = BuildConfigSetFallbackPayload(key, value, action_result.rc, action_result.stderr_text.empty()
            ? action_result.stdout_text
            : action_result.stderr_text);
        error_message.clear();
        return true;
    }

    if (IsObjectJsonText(action_result.payload_json) && action_result.payload_json != "{}") {
        json_payload = CompactObjectJsonOrDefault(action_result.payload_json);
    } else {
        json_payload = BuildConfigSetFallbackPayload(key, value, action_result.rc, action_result.stdout_text);
    }

    return true;
}

bool ControlScriptRunner::RunActionList(
    const std::string& app_id,
    std::string& json_payload,
    std::string& error_message) const {
    std::vector<ActionDefinition> actions;
    if (!LoadActionCatalog(repo_root_, path_templates_, app_id, actions, error_message)) {
        return false;
    }

    nlohmann::json response;
    response["actions"] = nlohmann::json::array();

    std::size_t index;
    for (index = 0; index < actions.size(); ++index) {
        nlohmann::json item;
        item["name"] = actions[index].name;
        item["label"] = actions[index].label;

        try {
            const nlohmann::json args = nlohmann::json::parse(actions[index].args_json);
            if (args.is_array()) {
                item["args"] = args;
            } else {
                item["args"] = nlohmann::json::array();
            }
        } catch (const std::exception&) {
            item["args"] = nlohmann::json::array();
        }

        response["actions"].push_back(item);
    }

    json_payload = response.dump();
    return true;
}

bool ControlScriptRunner::RunActionInvoke(
    const std::string& app_id,
    const std::string& action_name,
    const std::string& args_json,
    double timeout_seconds,
    std::string& json_payload,
    std::string& error_message) const {
    std::vector<ActionDefinition> actions;
    if (!LoadActionCatalog(repo_root_, path_templates_, app_id, actions, error_message)) {
        return false;
    }

    std::map<std::string, std::string> args_map;
    if (!ParseArgsObject(args_json, args_map, error_message)) {
        error_message = "bad args: " + error_message;
        return false;
    }

    const std::string accepted_at = CurrentUtcIso8601();

    ActionRunResult action_result;
    if (!ExecuteCatalogAction(repo_root_, actions, action_name, args_map, timeout_seconds, action_result, error_message)) {
        return false;
    }

    ActionJobRecord record;
    record.job_id = GenerateJobId();
    record.accepted_at = accepted_at;
    record.started_at = accepted_at;
    record.finished_at = CurrentUtcIso8601();
    record.result_json = CompactObjectJsonOrDefault(action_result.payload_json);
    record.stdout_text = action_result.stdout_text;
    record.stderr_text = action_result.stderr_text;

    if (action_result.ok) {
        record.state = "succeeded";
        record.has_error = false;
        record.error_code.clear();
        record.error_message.clear();
    } else if (action_result.timed_out || action_result.error_code == "action_timeout") {
        record.state = "timeout";
        record.has_error = true;
        record.error_code = "E_ACTION_TIMEOUT";
        record.error_message = action_result.error_message.empty() ? "action timed out" : action_result.error_message;
    } else {
        record.state = "failed";
        record.has_error = true;
        record.error_code = action_result.error_code.empty() ? "E_ACTION_FAILED" : action_result.error_code;
        record.error_message = action_result.error_message.empty() ? "action failed" : action_result.error_message;
    }

    if (!WriteActionJobRecord(repo_root_, path_templates_, app_id, record, error_message)) {
        return false;
    }

    json_payload = BuildActionInvokeAcceptedResponse(record.job_id, accepted_at);
    error_message.clear();
    return true;
}

bool ControlScriptRunner::RunActionJobGet(
    const std::string& app_id,
    const std::string& job_id,
    std::string& json_payload,
    std::string& error_message) const {
    ActionJobRecord record;
    if (!ReadActionJobRecord(repo_root_, path_templates_, app_id, job_id, record, error_message)) {
        return false;
    }

    json_payload = BuildActionJobResponse(record);
    error_message.clear();
    return true;
}

ControlScriptRunner CreateControlScriptRunner(
    const std::string& repo_root,
    const Common::PathTemplateSet& path_templates) {
    return ControlScriptRunner(repo_root, path_templates);
}

}  // namespace Common
}  // namespace ProcessInterface
