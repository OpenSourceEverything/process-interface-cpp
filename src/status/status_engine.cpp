#include "status_engine.h"

#include <map>
#include <sstream>

#include "../../external/nlohmann/json.hpp"
#include "../common/text.h"
#include "debug.h"
#include "status_operation_registry.h"

namespace ProcessInterface {
namespace Status {

namespace {

nlohmann::json FieldOrDefault(
    const std::map<std::string, nlohmann::json>& values,
    const std::string& key,
    const nlohmann::json& default_value) {
    const std::map<std::string, nlohmann::json>::const_iterator iter = values.find(key);
    if (iter == values.end()) {
        return default_value;
    }
    return iter->second;
}

bool ParseBoolText(const std::string& text, bool default_value) {
    const std::string trimmed = ProcessInterface::Common::TrimCopy(text);
    if (trimmed == "true" || trimmed == "TRUE" || trimmed == "1") {
        return true;
    }
    if (trimmed == "false" || trimmed == "FALSE" || trimmed == "0") {
        return false;
    }
    return default_value;
}

bool JsonToBool(const nlohmann::json& value, bool default_value) {
    if (value.is_boolean()) {
        return value.get<bool>();
    }
    if (value.is_number_integer()) {
        return value.get<int>() != 0;
    }
    if (value.is_string()) {
        return ParseBoolText(value.get<std::string>(), default_value);
    }
    return default_value;
}

bool JsonToInt(const nlohmann::json& value, int& out_value) {
    if (value.is_number_integer()) {
        out_value = value.get<int>();
        return true;
    }
    if (value.is_string()) {
        std::istringstream parser(ProcessInterface::Common::TrimCopy(value.get<std::string>()));
        parser >> out_value;
        return parser && parser.eof();
    }
    return false;
}

}  // namespace

StatusErrorCode ExecuteStatusSpec(
    const StatusSpec& spec,
    const StatusContext& context,
    std::string& payload_json,
    std::string& error_message) {
    std::map<std::string, nlohmann::json> values;
    nlohmann::json payload_fields = nlohmann::json::object();

    std::size_t index = 0;
    for (index = 0; index < spec.operations.size(); ++index) {
        const ParsedOperation& operation = spec.operations[index];
        nlohmann::json value_json;
        const StatusErrorCode rc = EvaluateOperation(operation, values, context, value_json, error_message);
        if (rc != StatusErrorCode::kNone) {
            error_message = "operation " + operation.field_name + " failed: " + error_message;
            return rc;
        }

        values[operation.field_name] = value_json;
        if (!operation.field_name.empty() && operation.field_name[0] != '_') {
            payload_fields[operation.field_name] = value_json;
        }
    }

    const bool running = JsonToBool(FieldOrDefault(values, spec.running_field, false), false);
    const bool host_running = JsonToBool(FieldOrDefault(values, spec.host_running_field, false), false);

    int pid = 0;
    const bool has_pid = JsonToInt(FieldOrDefault(values, spec.pid_field, nullptr), pid);

    int host_pid = 0;
    const bool has_host_pid = JsonToInt(FieldOrDefault(values, spec.host_pid_field, nullptr), host_pid);

    payload_fields["interfaceName"] = "generic-process-interface";
    payload_fields["interfaceVersion"] = 1;
    payload_fields["appId"] = spec.app_id;
    payload_fields["appTitle"] = spec.app_title;
    payload_fields["running"] = running;
    payload_fields["pid"] = has_pid ? nlohmann::json(pid) : nlohmann::json(nullptr);
    payload_fields["hostRunning"] = host_running;
    payload_fields["hostPid"] = has_host_pid ? nlohmann::json(host_pid) : nlohmann::json(nullptr);
    payload_fields["bootId"] = (running && has_pid) ? (spec.app_id + ":" + std::to_string(pid)) : std::string();
    payload_fields["error"] = std::string();

    payload_json = payload_fields.dump();

    if (DebugEnabled()) {
        std::ostringstream keys_stream;
        keys_stream << "appId=" << spec.app_id << " payloadKeys=[";

        bool first = true;
        nlohmann::json::const_iterator iter;
        for (iter = payload_fields.begin(); iter != payload_fields.end(); ++iter) {
            if (!first) {
                keys_stream << ",";
            }
            first = false;
            keys_stream << iter.key();
        }

        keys_stream << "]";
        DebugLog(keys_stream.str());
    }

    return StatusErrorCode::kNone;
}

}  // namespace Status
}  // namespace ProcessInterface
