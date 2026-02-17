#include "engine.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "debug.h"

namespace ProcessInterface {
namespace Status {

namespace {

std::string Trim(const std::string& value) {
    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
        ++start;
    }
    if (start >= value.size()) {
        return std::string();
    }
    std::size_t end = value.size() - 1;
    while (end > start && std::isspace(static_cast<unsigned char>(value[end])) != 0) {
        --end;
    }
    return value.substr(start, end - start + 1);
}

std::vector<std::string> Split(const std::string& value, char delimiter) {
    std::vector<std::string> parts;
    std::string current;
    std::size_t index;
    for (index = 0; index < value.size(); ++index) {
        const char c = value[index];
        if (c == delimiter) {
            parts.push_back(current);
            current.clear();
            continue;
        }
        current.push_back(c);
    }
    parts.push_back(current);
    return parts;
}

std::string Join(const std::vector<std::string>& parts, std::size_t start_index, const std::string& delimiter) {
    if (start_index >= parts.size()) {
        return std::string();
    }
    std::ostringstream stream;
    std::size_t index;
    for (index = start_index; index < parts.size(); ++index) {
        if (index > start_index) {
            stream << delimiter;
        }
        stream << parts[index];
    }
    return stream.str();
}

std::string JsonEscape(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() + 16);
    std::size_t index;
    for (index = 0; index < value.size(); ++index) {
        const char c = value[index];
        switch (c) {
            case '\\':
                escaped += "\\\\";
                break;
            case '"':
                escaped += "\\\"";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped.push_back(c);
                break;
        }
    }
    return escaped;
}

std::string JsonQuote(const std::string& value) {
    return std::string("\"") + JsonEscape(value) + "\"";
}

bool LooksLikeJsonValue(const std::string& value) {
    const std::string trimmed = Trim(value);
    if (trimmed.empty()) {
        return false;
    }
    if (trimmed == "true" || trimmed == "false" || trimmed == "null") {
        return true;
    }
    if (trimmed.front() == '"' && trimmed.back() == '"') {
        return true;
    }
    if (trimmed.front() == '{' && trimmed.back() == '}') {
        return true;
    }
    if (trimmed.front() == '[' && trimmed.back() == ']') {
        return true;
    }
    std::size_t index = 0;
    if (trimmed[index] == '-') {
        ++index;
    }
    bool has_digit = false;
    while (index < trimmed.size() && std::isdigit(static_cast<unsigned char>(trimmed[index])) != 0) {
        has_digit = true;
        ++index;
    }
    if (index < trimmed.size() && trimmed[index] == '.') {
        ++index;
        while (index < trimmed.size() && std::isdigit(static_cast<unsigned char>(trimmed[index])) != 0) {
            has_digit = true;
            ++index;
        }
    }
    return has_digit && index == trimmed.size();
}

bool ParseBoolLiteral(const std::string& value, bool default_value) {
    const std::string trimmed = Trim(value);
    if (trimmed == "true" || trimmed == "TRUE" || trimmed == "1") {
        return true;
    }
    if (trimmed == "false" || trimmed == "FALSE" || trimmed == "0") {
        return false;
    }
    return default_value;
}

bool ParseIntLiteral(const std::string& value, int& out_value) {
    const std::string trimmed = Trim(value);
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

bool ReadTextFile(const fs::path& path, std::string& text_out) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return false;
    }
    std::ostringstream stream;
    stream << input.rdbuf();
    text_out = stream.str();
    return true;
}

std::string BuildProcessProbeJson(const ProcessProbeResult& probe) {
    std::ostringstream stream;
    stream << "{";
    stream << "\"running\":" << (probe.running ? "true" : "false") << ",";
    if (probe.running && probe.pid > 0) {
        stream << "\"pid\":" << probe.pid << ",";
    } else {
        stream << "\"pid\":null,";
    }
    stream << "\"pids\":[";
    std::size_t index;
    for (index = 0; index < probe.pids.size(); ++index) {
        if (index > 0) {
            stream << ",";
        }
        stream << probe.pids[index];
    }
    stream << "]";
    stream << "}";
    return stream.str();
}

bool FindObjectKeyValueStart(const std::string& source, const std::string& key, std::size_t& value_start) {
    const std::string pattern_text = "\"" + key + "\"\\s*:\\s*";
    const std::regex pattern(pattern_text);
    std::smatch match;
    if (!std::regex_search(source, match, pattern)) {
        return false;
    }
    value_start = static_cast<std::size_t>(match.position(0) + match.length(0));
    return true;
}

void SkipWhitespace(const std::string& source, std::size_t& index) {
    while (index < source.size() && std::isspace(static_cast<unsigned char>(source[index])) != 0) {
        ++index;
    }
}

bool ExtractJsonValueFromObject(const std::string& object_json, const std::string& key, std::string& value_out) {
    std::size_t index = 0;
    if (!FindObjectKeyValueStart(object_json, key, index)) {
        return false;
    }
    SkipWhitespace(object_json, index);
    if (index >= object_json.size()) {
        return false;
    }

    const char first = object_json[index];
    if (first == '"') {
        bool escaped = false;
        std::size_t end = index + 1;
        for (; end < object_json.size(); ++end) {
            const char c = object_json[end];
            if (escaped) {
                escaped = false;
                continue;
            }
            if (c == '\\') {
                escaped = true;
                continue;
            }
            if (c == '"') {
                value_out = object_json.substr(index, end - index + 1);
                return true;
            }
        }
        return false;
    }

    if (first == '{' || first == '[') {
        const char open_char = first;
        const char close_char = (open_char == '{') ? '}' : ']';
        int depth = 0;
        bool in_string = false;
        bool escaped = false;
        std::size_t end = index;
        for (; end < object_json.size(); ++end) {
            const char c = object_json[end];
            if (in_string) {
                if (escaped) {
                    escaped = false;
                } else if (c == '\\') {
                    escaped = true;
                } else if (c == '"') {
                    in_string = false;
                }
                continue;
            }
            if (c == '"') {
                in_string = true;
                continue;
            }
            if (c == open_char) {
                ++depth;
                continue;
            }
            if (c == close_char) {
                --depth;
                if (depth == 0) {
                    value_out = object_json.substr(index, end - index + 1);
                    return true;
                }
            }
        }
        return false;
    }

    std::size_t end = index;
    while (end < object_json.size()) {
        const char c = object_json[end];
        if (c == ',' || c == '}' || c == ']' || std::isspace(static_cast<unsigned char>(c)) != 0) {
            break;
        }
        ++end;
    }
    if (end <= index) {
        return false;
    }
    value_out = object_json.substr(index, end - index);
    return true;
}

bool JsonLiteralToBool(const std::string& value, bool default_value) {
    const std::string trimmed = Trim(value);
    if (trimmed == "true") {
        return true;
    }
    if (trimmed == "false") {
        return false;
    }
    return default_value;
}

bool JsonLiteralToInt(const std::string& value, int& out_value) {
    const std::string trimmed = Trim(value);
    if (trimmed.empty() || trimmed == "null") {
        return false;
    }
    return ParseIntLiteral(trimmed, out_value);
}

std::string SerializeObject(const std::map<std::string, std::string>& fields) {
    std::ostringstream stream;
    stream << "{";
    bool first = true;
    std::map<std::string, std::string>::const_iterator iter;
    for (iter = fields.begin(); iter != fields.end(); ++iter) {
        if (!first) {
            stream << ",";
        }
        first = false;
        stream << JsonQuote(iter->first) << ":" << iter->second;
    }
    stream << "}";
    return stream.str();
}

std::string FieldOrDefault(
    const std::map<std::string, std::string>& values,
    const std::string& key,
    const std::string& default_json) {
    const std::map<std::string, std::string>::const_iterator iter = values.find(key);
    if (iter == values.end()) {
        return default_json;
    }
    return iter->second;
}

StatusErrorCode EvaluateOperation(
    const std::string& op_name,
    const std::vector<std::string>& args,
    const std::map<std::string, std::string>& values,
    const StatusContext& context,
    std::string& out_json,
    std::string& error_message) {
    if (op_name == "const") {
        const std::string literal = Trim(Join(args, 0, ":"));
        if (!LooksLikeJsonValue(literal)) {
            error_message = "const op requires JSON literal";
            return StatusErrorCode::kSpecInvalid;
        }
        out_json = literal;
        return StatusErrorCode::kNone;
    }

    if (op_name == "const_str") {
        out_json = JsonQuote(Join(args, 0, ":"));
        return StatusErrorCode::kNone;
    }

    if (op_name == "file_json") {
        if (args.empty()) {
            error_message = "file_json requires path argument";
            return StatusErrorCode::kSpecInvalid;
        }
        const fs::path path = context.repo_root / args[0];
        const std::string default_json = args.size() > 1 ? Trim(Join(args, 1, ":")) : "{}";
        std::string text;
        if (!ReadTextFile(path, text)) {
            out_json = default_json;
            DebugLog("file_json missing path=" + path.string());
            return StatusErrorCode::kNone;
        }
        const std::string trimmed = Trim(text);
        if (LooksLikeJsonValue(trimmed) && (trimmed.front() == '{' || trimmed.front() == '[')) {
            out_json = trimmed;
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
        const fs::path path = context.repo_root / args[0];
        out_json = fs::exists(path) ? "true" : "false";
        return StatusErrorCode::kNone;
    }

    if (op_name == "process_running") {
        if (args.empty()) {
            error_message = "process_running requires process name";
            return StatusErrorCode::kSpecInvalid;
        }
        const ProcessProbeResult probe = QueryProcessByName(args[0]);
        out_json = BuildProcessProbeJson(probe);
        return StatusErrorCode::kNone;
    }

    if (op_name == "port_listening") {
        if (args.size() < 2) {
            error_message = "port_listening requires host and port";
            return StatusErrorCode::kSpecInvalid;
        }
        int port = 0;
        if (!ParseIntLiteral(args[1], port)) {
            error_message = "port_listening invalid port";
            return StatusErrorCode::kSpecInvalid;
        }
        int timeout_ms = 250;
        if (args.size() > 2) {
            ParseIntLiteral(args[2], timeout_ms);
        }
        const bool listening = CheckPortListening(args[0], port, timeout_ms);
        out_json = listening ? "true" : "false";
        return StatusErrorCode::kNone;
    }

    if (op_name == "derive") {
        if (args.empty()) {
            error_message = "derive requires sub-operation";
            return StatusErrorCode::kSpecInvalid;
        }
        const std::string sub = args[0];
        if (sub == "copy") {
            if (args.size() < 2) {
                error_message = "derive copy requires source field";
                return StatusErrorCode::kSpecInvalid;
            }
            out_json = FieldOrDefault(values, args[1], "null");
            return StatusErrorCode::kNone;
        }
        if (sub == "bool_from_obj") {
            if (args.size() < 3) {
                error_message = "derive bool_from_obj requires source and key";
                return StatusErrorCode::kSpecInvalid;
            }
            const std::string source_json = FieldOrDefault(values, args[1], "{}");
            std::string field_json;
            bool value = false;
            if (ExtractJsonValueFromObject(source_json, args[2], field_json)) {
                value = JsonLiteralToBool(field_json, false);
            } else if (args.size() > 3) {
                value = ParseBoolLiteral(args[3], false);
            }
            out_json = value ? "true" : "false";
            return StatusErrorCode::kNone;
        }
        if (sub == "int_from_obj") {
            if (args.size() < 3) {
                error_message = "derive int_from_obj requires source and key";
                return StatusErrorCode::kSpecInvalid;
            }
            const std::string source_json = FieldOrDefault(values, args[1], "{}");
            std::string field_json;
            int parsed = 0;
            if (ExtractJsonValueFromObject(source_json, args[2], field_json) && JsonLiteralToInt(field_json, parsed)) {
                out_json = std::to_string(parsed);
            } else {
                out_json = "null";
            }
            return StatusErrorCode::kNone;
        }
        if (sub == "str_from_obj") {
            if (args.size() < 3) {
                error_message = "derive str_from_obj requires source and key";
                return StatusErrorCode::kSpecInvalid;
            }
            const std::string source_json = FieldOrDefault(values, args[1], "{}");
            const std::string default_value = (args.size() > 3) ? args[3] : "";
            std::string field_json;
            if (ExtractJsonValueFromObject(source_json, args[2], field_json)) {
                const std::string trimmed = Trim(field_json);
                if (trimmed.size() >= 2 && trimmed.front() == '"' && trimmed.back() == '"') {
                    out_json = trimmed;
                    return StatusErrorCode::kNone;
                }
            }
            out_json = JsonQuote(default_value);
            return StatusErrorCode::kNone;
        }
        if (sub == "json_from_obj") {
            if (args.size() < 3) {
                error_message = "derive json_from_obj requires source and key";
                return StatusErrorCode::kSpecInvalid;
            }
            const std::string source_json = FieldOrDefault(values, args[1], "{}");
            std::string field_json;
            if (ExtractJsonValueFromObject(source_json, args[2], field_json)) {
                out_json = Trim(field_json);
            } else if (args.size() > 3) {
                out_json = Trim(args[3]);
            } else {
                out_json = "null";
            }
            if (!LooksLikeJsonValue(out_json)) {
                out_json = "null";
            }
            return StatusErrorCode::kNone;
        }
        if (sub == "running_display") {
            if (args.size() < 3) {
                error_message = "derive running_display requires running and pid fields";
                return StatusErrorCode::kSpecInvalid;
            }
            const bool running = JsonLiteralToBool(FieldOrDefault(values, args[1], "false"), false);
            int pid = 0;
            const bool has_pid = JsonLiteralToInt(FieldOrDefault(values, args[2], "null"), pid);
            if (running && has_pid) {
                out_json = JsonQuote("True (PID " + std::to_string(pid) + ")");
            } else if (running) {
                out_json = JsonQuote("True");
            } else {
                out_json = JsonQuote("False");
            }
            return StatusErrorCode::kNone;
        }
        if (sub == "str_if_bool") {
            if (args.size() < 4) {
                error_message = "derive str_if_bool requires bool field and true/false text";
                return StatusErrorCode::kSpecInvalid;
            }
            const bool value = JsonLiteralToBool(FieldOrDefault(values, args[1], "false"), false);
            out_json = JsonQuote(value ? args[2] : args[3]);
            return StatusErrorCode::kNone;
        }
        if (sub == "pick_int") {
            if (args.size() < 3) {
                error_message = "derive pick_int requires primary and fallback fields";
                return StatusErrorCode::kSpecInvalid;
            }
            int primary = 0;
            if (JsonLiteralToInt(FieldOrDefault(values, args[1], "null"), primary)) {
                out_json = std::to_string(primary);
                return StatusErrorCode::kNone;
            }
            int fallback = 0;
            if (JsonLiteralToInt(FieldOrDefault(values, args[2], "null"), fallback)) {
                out_json = std::to_string(fallback);
                return StatusErrorCode::kNone;
            }
            out_json = "null";
            return StatusErrorCode::kNone;
        }
        if (sub == "or_bool") {
            if (args.size() < 3) {
                error_message = "derive or_bool requires two bool fields";
                return StatusErrorCode::kSpecInvalid;
            }
            const bool left = JsonLiteralToBool(FieldOrDefault(values, args[1], "false"), false);
            const bool right = JsonLiteralToBool(FieldOrDefault(values, args[2], "false"), false);
            out_json = (left || right) ? "true" : "false";
            return StatusErrorCode::kNone;
        }

        error_message = "unsupported derive operation: " + sub;
        return StatusErrorCode::kSpecInvalid;
    }

    error_message = "unsupported operation: " + op_name;
    return StatusErrorCode::kSpecInvalid;
}

}  // namespace

StatusErrorCode ExecuteStatusSpec(
    const StatusSpec& spec,
    const StatusContext& context,
    std::string& payload_json,
    std::string& error_message) {
    std::map<std::string, std::string> values;
    std::map<std::string, std::string> payload_fields;

    std::size_t index;
    for (index = 0; index < spec.operations.size(); ++index) {
        const std::string line = Trim(spec.operations[index]);
        if (line.empty()) {
            continue;
        }
        const std::size_t equal_index = line.find('=');
        if (equal_index == std::string::npos || equal_index == 0 || equal_index + 1 >= line.size()) {
            error_message = "invalid operation line: " + line;
            return StatusErrorCode::kSpecInvalid;
        }

        const std::string field_name = Trim(line.substr(0, equal_index));
        const std::string expression = Trim(line.substr(equal_index + 1));
        if (field_name.empty()) {
            error_message = "operation field is empty";
            return StatusErrorCode::kSpecInvalid;
        }
        const std::vector<std::string> parts = Split(expression, ':');
        if (parts.empty()) {
            error_message = "operation expression is empty";
            return StatusErrorCode::kSpecInvalid;
        }
        const std::string op_name = Trim(parts[0]);
        std::vector<std::string> args;
        std::size_t part_index;
        for (part_index = 1; part_index < parts.size(); ++part_index) {
            args.push_back(parts[part_index]);
        }

        std::string value_json;
        const StatusErrorCode rc = EvaluateOperation(op_name, args, values, context, value_json, error_message);
        if (rc != StatusErrorCode::kNone) {
            error_message = "operation " + field_name + " failed: " + error_message;
            return rc;
        }
        values[field_name] = value_json;
        if (!field_name.empty() && field_name[0] != '_') {
            payload_fields[field_name] = value_json;
        }
    }

    const bool running = JsonLiteralToBool(FieldOrDefault(values, spec.running_field, "false"), false);
    const bool host_running = JsonLiteralToBool(FieldOrDefault(values, spec.host_running_field, "false"), false);
    int pid = 0;
    const bool has_pid = JsonLiteralToInt(FieldOrDefault(values, spec.pid_field, "null"), pid);
    int host_pid = 0;
    const bool has_host_pid = JsonLiteralToInt(FieldOrDefault(values, spec.host_pid_field, "null"), host_pid);

    payload_fields["interfaceName"] = JsonQuote("generic-process-interface");
    payload_fields["interfaceVersion"] = "1";
    payload_fields["appId"] = JsonQuote(spec.app_id);
    payload_fields["appTitle"] = JsonQuote(spec.app_title);
    payload_fields["running"] = running ? "true" : "false";
    payload_fields["pid"] = has_pid ? std::to_string(pid) : "null";
    payload_fields["hostRunning"] = host_running ? "true" : "false";
    payload_fields["hostPid"] = has_host_pid ? std::to_string(host_pid) : "null";
    payload_fields["bootId"] = (running && has_pid) ? JsonQuote(spec.app_id + ":" + std::to_string(pid)) : JsonQuote("");
    payload_fields["error"] = JsonQuote("");

    payload_json = SerializeObject(payload_fields);

    if (DebugEnabled()) {
        std::ostringstream keys_stream;
        keys_stream << "appId=" << spec.app_id << " payloadKeys=[";
        bool first = true;
        std::map<std::string, std::string>::const_iterator iter;
        for (iter = payload_fields.begin(); iter != payload_fields.end(); ++iter) {
            if (!first) {
                keys_stream << ",";
            }
            first = false;
            keys_stream << iter->first;
        }
        keys_stream << "]";
        DebugLog(keys_stream.str());
    }

    return StatusErrorCode::kNone;
}

}  // namespace Status
}  // namespace ProcessInterface

