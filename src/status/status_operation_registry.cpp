#include "status_operation_registry.h"

#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "../common/file_io.h"
#include "../common/text.h"
#include "debug.h"

namespace ProcessInterface {
namespace Status {

namespace {

bool ParseIntText(const std::string& text, int& out_value) {
    const std::string trimmed = ProcessInterface::Common::TrimCopy(text);
    if (trimmed.empty()) {
        return false;
    }

    std::istringstream parser(trimmed);
    int parsed = 0;
    parser >> parsed;
    if (!parser || !parser.eof()) {
        return false;
    }

    out_value = parsed;
    return true;
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

bool TryParseJsonLiteral(const std::string& text, nlohmann::json& value_out) {
    try {
        value_out = nlohmann::json::parse(text);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

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
        return ParseIntText(value.get<std::string>(), out_value);
    }
    return false;
}

nlohmann::json BuildProcessProbeJson(const ProcessProbeResult& probe) {
    nlohmann::json payload;
    payload["running"] = probe.running;
    payload["pid"] = probe.running && probe.pid > 0 ? nlohmann::json(probe.pid) : nlohmann::json(nullptr);

    nlohmann::json pids = nlohmann::json::array();
    std::size_t index = 0;
    for (index = 0; index < probe.pids.size(); ++index) {
        pids.push_back(probe.pids[index]);
    }
    payload["pids"] = pids;
    return payload;
}

}  // namespace

StatusErrorCode EvaluateOperation(
    const ParsedOperation& operation,
    const std::map<std::string, nlohmann::json>& values,
    const StatusContext& context,
    nlohmann::json& out_json,
    std::string& error_message) {
    const std::string op_name = operation.op_name;
    const std::vector<std::string>& args = operation.args;

    if (op_name == "const") {
        const std::string literal = ProcessInterface::Common::TrimCopy(ProcessInterface::Common::Join(args, 0, ":"));
        if (!TryParseJsonLiteral(literal, out_json)) {
            error_message = "const op requires JSON literal";
            return StatusErrorCode::kSpecInvalid;
        }
        return StatusErrorCode::kNone;
    }

    if (op_name == "const_str") {
        out_json = ProcessInterface::Common::Join(args, 0, ":");
        return StatusErrorCode::kNone;
    }

    if (op_name == "file_json") {
        if (args.empty()) {
            error_message = "file_json requires path argument";
            return StatusErrorCode::kSpecInvalid;
        }

        const fs::path path = context.repo_root / ProcessInterface::Common::TrimCopy(args[0]);

        nlohmann::json default_json = nlohmann::json::object();
        if (args.size() > 1) {
            const std::string raw_default = ProcessInterface::Common::TrimCopy(ProcessInterface::Common::Join(args, 1, ":"));
            if (!raw_default.empty()) {
                TryParseJsonLiteral(raw_default, default_json);
            }
        }

        std::string text;
        if (!ProcessInterface::Common::ReadTextFile(path, text)) {
            out_json = default_json;
            DebugLog("file_json missing path=" + path.string());
            return StatusErrorCode::kNone;
        }

        nlohmann::json parsed;
        if (TryParseJsonLiteral(ProcessInterface::Common::TrimCopy(text), parsed) && (parsed.is_object() || parsed.is_array())) {
            out_json = parsed;
            return StatusErrorCode::kNone;
        }

        out_json = default_json;
        return StatusErrorCode::kNone;
    }

    if (op_name == "file_exists") {
        if (args.empty()) {
            error_message = "file_exists requires path argument";
            return StatusErrorCode::kSpecInvalid;
        }
        const fs::path path = context.repo_root / ProcessInterface::Common::TrimCopy(args[0]);
        out_json = fs::exists(path);
        return StatusErrorCode::kNone;
    }

    if (op_name == "process_running") {
        if (args.empty()) {
            error_message = "process_running requires process name";
            return StatusErrorCode::kSpecInvalid;
        }
        if (context.probes == NULL) {
            error_message = "status probes are not available";
            return StatusErrorCode::kCollectFailed;
        }
        const ProcessProbeResult probe = context.probes->QueryProcessByName(ProcessInterface::Common::TrimCopy(args[0]));
        out_json = BuildProcessProbeJson(probe);
        return StatusErrorCode::kNone;
    }

    if (op_name == "port_listening") {
        if (args.size() < 2) {
            error_message = "port_listening requires host and port";
            return StatusErrorCode::kSpecInvalid;
        }
        if (context.probes == NULL) {
            error_message = "status probes are not available";
            return StatusErrorCode::kCollectFailed;
        }

        int port = 0;
        if (!ParseIntText(args[1], port)) {
            error_message = "port_listening invalid port";
            return StatusErrorCode::kSpecInvalid;
        }

        int timeout_ms = 250;
        if (args.size() > 2) {
            ParseIntText(args[2], timeout_ms);
        }

        out_json = context.probes->CheckPortListening(ProcessInterface::Common::TrimCopy(args[0]), port, timeout_ms);
        return StatusErrorCode::kNone;
    }

    if (op_name == "derive") {
        if (args.empty()) {
            error_message = "derive requires sub-operation";
            return StatusErrorCode::kSpecInvalid;
        }

        const std::string sub = ProcessInterface::Common::TrimCopy(args[0]);

        if (sub == "copy") {
            if (args.size() < 2) {
                error_message = "derive copy requires source field";
                return StatusErrorCode::kSpecInvalid;
            }
            out_json = FieldOrDefault(values, ProcessInterface::Common::TrimCopy(args[1]), nullptr);
            return StatusErrorCode::kNone;
        }

        if (sub == "bool_from_obj") {
            if (args.size() < 3) {
                error_message = "derive bool_from_obj requires source and key";
                return StatusErrorCode::kSpecInvalid;
            }

            const nlohmann::json source = FieldOrDefault(values, ProcessInterface::Common::TrimCopy(args[1]), nlohmann::json::object());
            const std::string key = ProcessInterface::Common::TrimCopy(args[2]);

            bool value = false;
            if (source.is_object() && source.contains(key)) {
                value = JsonToBool(source[key], false);
            } else if (args.size() > 3) {
                value = ParseBoolText(args[3], false);
            }

            out_json = value;
            return StatusErrorCode::kNone;
        }

        if (sub == "int_from_obj") {
            if (args.size() < 3) {
                error_message = "derive int_from_obj requires source and key";
                return StatusErrorCode::kSpecInvalid;
            }

            const nlohmann::json source = FieldOrDefault(values, ProcessInterface::Common::TrimCopy(args[1]), nlohmann::json::object());
            const std::string key = ProcessInterface::Common::TrimCopy(args[2]);

            int parsed = 0;
            if (source.is_object() && source.contains(key) && JsonToInt(source[key], parsed)) {
                out_json = parsed;
            } else {
                out_json = nullptr;
            }
            return StatusErrorCode::kNone;
        }

        if (sub == "str_from_obj") {
            if (args.size() < 3) {
                error_message = "derive str_from_obj requires source and key";
                return StatusErrorCode::kSpecInvalid;
            }

            const nlohmann::json source = FieldOrDefault(values, ProcessInterface::Common::TrimCopy(args[1]), nlohmann::json::object());
            const std::string key = ProcessInterface::Common::TrimCopy(args[2]);
            const std::string default_value = args.size() > 3 ? args[3] : std::string();

            if (source.is_object() && source.contains(key) && source[key].is_string()) {
                out_json = source[key].get<std::string>();
            } else {
                out_json = default_value;
            }
            return StatusErrorCode::kNone;
        }

        if (sub == "json_from_obj") {
            if (args.size() < 3) {
                error_message = "derive json_from_obj requires source and key";
                return StatusErrorCode::kSpecInvalid;
            }

            const nlohmann::json source = FieldOrDefault(values, ProcessInterface::Common::TrimCopy(args[1]), nlohmann::json::object());
            const std::string key = ProcessInterface::Common::TrimCopy(args[2]);

            if (source.is_object() && source.contains(key)) {
                out_json = source[key];
            } else if (args.size() > 3) {
                nlohmann::json fallback;
                if (TryParseJsonLiteral(ProcessInterface::Common::TrimCopy(args[3]), fallback)) {
                    out_json = fallback;
                } else {
                    out_json = nullptr;
                }
            } else {
                out_json = nullptr;
            }
            return StatusErrorCode::kNone;
        }

        if (sub == "running_display") {
            if (args.size() < 3) {
                error_message = "derive running_display requires running and pid fields";
                return StatusErrorCode::kSpecInvalid;
            }

            const bool running = JsonToBool(FieldOrDefault(values, ProcessInterface::Common::TrimCopy(args[1]), false), false);
            int pid = 0;
            const bool has_pid = JsonToInt(FieldOrDefault(values, ProcessInterface::Common::TrimCopy(args[2]), nullptr), pid);

            if (running && has_pid) {
                out_json = std::string("True (PID ") + std::to_string(pid) + ")";
            } else if (running) {
                out_json = "True";
            } else {
                out_json = "False";
            }
            return StatusErrorCode::kNone;
        }

        if (sub == "str_if_bool") {
            if (args.size() < 4) {
                error_message = "derive str_if_bool requires bool field and true/false text";
                return StatusErrorCode::kSpecInvalid;
            }

            const bool value = JsonToBool(FieldOrDefault(values, ProcessInterface::Common::TrimCopy(args[1]), false), false);
            out_json = value ? args[2] : args[3];
            return StatusErrorCode::kNone;
        }

        if (sub == "pick_int") {
            if (args.size() < 3) {
                error_message = "derive pick_int requires primary and fallback fields";
                return StatusErrorCode::kSpecInvalid;
            }

            int primary = 0;
            if (JsonToInt(FieldOrDefault(values, ProcessInterface::Common::TrimCopy(args[1]), nullptr), primary)) {
                out_json = primary;
                return StatusErrorCode::kNone;
            }

            int fallback = 0;
            if (JsonToInt(FieldOrDefault(values, ProcessInterface::Common::TrimCopy(args[2]), nullptr), fallback)) {
                out_json = fallback;
                return StatusErrorCode::kNone;
            }

            out_json = nullptr;
            return StatusErrorCode::kNone;
        }

        if (sub == "or_bool") {
            if (args.size() < 3) {
                error_message = "derive or_bool requires two bool fields";
                return StatusErrorCode::kSpecInvalid;
            }

            const bool left = JsonToBool(FieldOrDefault(values, ProcessInterface::Common::TrimCopy(args[1]), false), false);
            const bool right = JsonToBool(FieldOrDefault(values, ProcessInterface::Common::TrimCopy(args[2]), false), false);
            out_json = left || right;
            return StatusErrorCode::kNone;
        }

        error_message = "unsupported derive operation: " + sub;
        return StatusErrorCode::kSpecInvalid;
    }

    error_message = "unsupported operation: " + op_name;
    return StatusErrorCode::kSpecInvalid;
}

}  // namespace Status
}  // namespace ProcessInterface
