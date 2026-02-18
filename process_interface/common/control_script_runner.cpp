#include "control_script_runner.h"

#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <direct.h>
#include <fstream>
#include <io.h>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace ProcessInterface {
namespace Common {

namespace {

struct CatalogAction {
    std::string name;
    std::string label;
    std::vector<std::string> command;
    std::string cwd;
    double timeout_seconds;
    bool detached;
    std::string args_json;
};

std::string Trim(const std::string& value) {
    if (value.empty()) {
        return std::string();
    }
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

std::string CompactJson(const std::string& text) {
    std::string output;
    output.reserve(text.size());

    bool in_string = false;
    bool escaped = false;
    std::size_t index;
    for (index = 0; index < text.size(); ++index) {
        const char c = text[index];
        if (in_string) {
            output.push_back(c);
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
            output.push_back(c);
            continue;
        }

        if (c != '\r' && c != '\n' && c != '\t' && c != ' ') {
            output.push_back(c);
        }
    }
    return output;
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

std::string QuoteForCommandArg(const std::string& value) {
    std::string output = "\"";
    std::size_t index;
    for (index = 0; index < value.size(); ++index) {
        const char c = value[index];
        if (c == '"') {
            output += "\"\"";
        } else {
            output.push_back(c);
        }
    }
    output += "\"";
    return output;
}

bool EnsureDirectory(const std::string& path) {
    if (path.empty()) {
        return false;
    }
    const int rc = _mkdir(path.c_str());
    if (rc == 0) {
        return true;
    }
    return errno == EEXIST;
}

std::string JoinPath(const std::string& base, const std::string& suffix) {
    if (base.empty()) {
        return suffix;
    }
    if (suffix.empty()) {
        return base;
    }
    const bool has_sep = base.back() == '\\' || base.back() == '/';
    if (has_sep) {
        return base + suffix;
    }
    return base + "\\" + suffix;
}

bool PathLooksAbsolute(const std::string& path) {
    if (path.size() >= 3 &&
        std::isalpha(static_cast<unsigned char>(path[0])) != 0 &&
        path[1] == ':' &&
        (path[2] == '\\' || path[2] == '/')) {
        return true;
    }
    if (path.size() >= 2 && path[0] == '\\' && path[1] == '\\') {
        return true;
    }
    return false;
}

bool PathExists(const std::string& path) {
    if (path.empty()) {
        return false;
    }
    return _access(path.c_str(), 0) == 0;
}

bool ReadTextFile(const std::string& path, std::string& text_out) {
    std::ifstream input(path.c_str(), std::ios::binary);
    if (!input.is_open()) {
        return false;
    }
    std::ostringstream stream;
    stream << input.rdbuf();
    text_out = stream.str();
    return true;
}

bool RunCommandCapture(
    const std::string& command_text,
    std::string& combined_output,
    int& exit_code,
    std::string& error_message) {
    FILE* process = _popen(command_text.c_str(), "r");
    if (process == NULL) {
        error_message = "failed to start command";
        return false;
    }

    std::ostringstream output_stream;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), process) != NULL) {
        output_stream << buffer;
    }
    exit_code = _pclose(process);
    combined_output = output_stream.str();
    return true;
}

bool FindJsonValueStart(const std::string& source, const std::string& key, std::size_t& value_start) {
    const std::regex pattern("\\\"" + key + "\\\"\\s*:\\s*");
    std::smatch match;
    if (!std::regex_search(source, match, pattern)) {
        return false;
    }
    value_start = static_cast<std::size_t>(match.position(0) + match.length(0));
    return true;
}

bool ExtractBalanced(const std::string& source, std::size_t start, std::size_t& end_out) {
    if (start >= source.size()) {
        return false;
    }
    const char first = source[start];
    if (first == '{' || first == '[') {
        const char open_char = first;
        const char close_char = (open_char == '{') ? '}' : ']';
        bool in_string = false;
        bool escaped = false;
        int depth = 0;
        std::size_t index;
        for (index = start; index < source.size(); ++index) {
            const char c = source[index];
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
                    end_out = index + 1;
                    return true;
                }
            }
        }
        return false;
    }

    if (first == '"') {
        bool escaped = false;
        std::size_t index;
        for (index = start + 1; index < source.size(); ++index) {
            const char c = source[index];
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                end_out = index + 1;
                return true;
            }
        }
        return false;
    }

    std::size_t index = start;
    while (index < source.size()) {
        const char c = source[index];
        if (c == ',' || c == '}' || c == ']' || std::isspace(static_cast<unsigned char>(c)) != 0) {
            break;
        }
        ++index;
    }
    if (index == start) {
        return false;
    }
    end_out = index;
    return true;
}

bool ExtractRawValue(const std::string& source, const std::string& key, std::string& value_out) {
    std::size_t start = 0;
    if (!FindJsonValueStart(source, key, start)) {
        return false;
    }
    while (start < source.size() && std::isspace(static_cast<unsigned char>(source[start])) != 0) {
        ++start;
    }
    std::size_t end = 0;
    if (!ExtractBalanced(source, start, end)) {
        return false;
    }
    value_out = Trim(source.substr(start, end - start));
    return true;
}

std::string UnquoteJsonString(const std::string& text) {
    const std::string trimmed = Trim(text);
    if (trimmed.size() < 2 || trimmed.front() != '"' || trimmed.back() != '"') {
        return trimmed;
    }
    std::string output;
    bool escaped = false;
    std::size_t index;
    for (index = 1; index + 1 < trimmed.size(); ++index) {
        const char c = trimmed[index];
        if (escaped) {
            switch (c) {
                case 'n':
                    output.push_back('\n');
                    break;
                case 'r':
                    output.push_back('\r');
                    break;
                case 't':
                    output.push_back('\t');
                    break;
                default:
                    output.push_back(c);
                    break;
            }
            escaped = false;
            continue;
        }
        if (c == '\\') {
            escaped = true;
            continue;
        }
        output.push_back(c);
    }
    return output;
}

bool ParseCommandArray(const std::string& array_text, std::vector<std::string>& command_out) {
    command_out.clear();
    const std::regex item_pattern("\"((?:[^\"\\\\]|\\\\.)*)\"");
    std::sregex_iterator begin(array_text.begin(), array_text.end(), item_pattern);
    std::sregex_iterator end;
    for (; begin != end; ++begin) {
        command_out.push_back(UnquoteJsonString(begin->str()));
    }
    return !command_out.empty();
}

bool ParseActionsCatalog(const std::string& catalog_text, std::vector<CatalogAction>& actions_out, std::string& error_message) {
    std::string actions_raw;
    if (!ExtractRawValue(catalog_text, "actions", actions_raw)) {
        error_message = "action catalog missing actions";
        return false;
    }
    const std::string actions_array = Trim(actions_raw);
    if (actions_array.size() < 2 || actions_array.front() != '[' || actions_array.back() != ']') {
        error_message = "action catalog actions must be array";
        return false;
    }

    actions_out.clear();
    std::size_t index = 1;
    while (index + 1 < actions_array.size()) {
        while (index + 1 < actions_array.size() && std::isspace(static_cast<unsigned char>(actions_array[index])) != 0) {
            ++index;
        }
        if (index + 1 >= actions_array.size() || actions_array[index] == ']') {
            break;
        }
        if (actions_array[index] == ',') {
            ++index;
            continue;
        }
        if (actions_array[index] != '{') {
            ++index;
            continue;
        }

        std::size_t object_end = 0;
        if (!ExtractBalanced(actions_array, index, object_end)) {
            error_message = "invalid action object";
            return false;
        }
        const std::string object_text = actions_array.substr(index, object_end - index);
        index = object_end;

        CatalogAction action;
        std::string name_raw;
        if (!ExtractRawValue(object_text, "name", name_raw)) {
            continue;
        }
        action.name = UnquoteJsonString(name_raw);
        if (action.name.empty()) {
            continue;
        }

        std::string label_raw;
        if (ExtractRawValue(object_text, "label", label_raw)) {
            action.label = UnquoteJsonString(label_raw);
        }
        if (action.label.empty()) {
            action.label = action.name;
        }

        std::string command_raw;
        if (!ExtractRawValue(object_text, "cmd", command_raw) || !ParseCommandArray(command_raw, action.command)) {
            continue;
        }

        std::string cwd_raw;
        if (ExtractRawValue(object_text, "cwd", cwd_raw)) {
            action.cwd = UnquoteJsonString(cwd_raw);
        }

        action.timeout_seconds = 30.0;
        std::string timeout_raw;
        if (ExtractRawValue(object_text, "timeoutSeconds", timeout_raw)) {
            std::istringstream parser(Trim(timeout_raw));
            double parsed = 0.0;
            parser >> parsed;
            if (parser && parsed > 0.0) {
                action.timeout_seconds = parsed;
            }
        }

        action.detached = false;
        std::string detached_raw;
        if (ExtractRawValue(object_text, "detached", detached_raw)) {
            const std::string lowered = Trim(detached_raw);
            action.detached = lowered == "true";
        }

        std::string args_raw;
        if (ExtractRawValue(object_text, "args", args_raw)) {
            const std::string compact = CompactJson(args_raw);
            if (compact.size() >= 2 && compact.front() == '[' && compact.back() == ']') {
                action.args_json = compact;
            }
        }
        if (action.args_json.empty()) {
            action.args_json = "[]";
        }

        actions_out.push_back(action);
    }

    if (actions_out.empty()) {
        error_message = "action catalog has no runnable actions";
        return false;
    }
    return true;
}

std::string BuildCommandLine(const std::vector<std::string>& command_parts) {
    std::ostringstream command_stream;
    std::size_t index;
    for (index = 0; index < command_parts.size(); ++index) {
        if (index > 0) {
            command_stream << " ";
        }
        command_stream << QuoteForCommandArg(command_parts[index]);
    }
    return command_stream.str();
}

}  // namespace

ControlScriptRunner::ControlScriptRunner(
    const std::string& repo_root,
    const std::string& script_relative_path,
    const std::string& script_label)
    : repo_root_(repo_root), script_relative_path_(script_relative_path), script_label_(script_label) {}

bool ControlScriptRunner::IsJsonObjectText(const std::string& text) {
    const std::string compact = CompactJson(text);
    return compact.size() >= 2 && compact.front() == '{' && compact.back() == '}';
}

bool ControlScriptRunner::TryExtractFirstJsonObject(const std::string& text, std::string& object_json_out) {
    std::size_t start = 0;
    while (start < text.size() && text[start] != '{') {
        ++start;
    }
    if (start >= text.size()) {
        return false;
    }
    std::size_t end = 0;
    if (!ExtractBalanced(text, start, end)) {
        return false;
    }
    object_json_out = text.substr(start, end - start);
    return IsJsonObjectText(object_json_out);
}

std::vector<std::string> ControlScriptRunner::SplitOutputLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        const std::string trimmed = Trim(line);
        if (!trimmed.empty()) {
            lines.push_back(trimmed);
        }
    }
    return lines;
}

std::string ControlScriptRunner::BuildConfigSetFallbackPayload(
    const std::string& key,
    const std::string& value,
    int rc,
    const std::string& stdout_text) {
    const std::vector<std::string> lines = SplitOutputLines(stdout_text);
    std::ostringstream stream;
    stream << "{";
    stream << "\"ok\":" << (rc == 0 ? "true" : "false") << ",";
    stream << "\"key\":" << JsonQuote(key) << ",";
    stream << "\"value\":" << JsonQuote(value) << ",";
    stream << "\"output\":[";
    std::size_t index;
    for (index = 0; index < lines.size(); ++index) {
        if (index > 0) {
            stream << ",";
        }
        stream << JsonQuote(lines[index]);
    }
    stream << "]";
    stream << "}";
    return stream.str();
}

bool ControlScriptRunner::LoadActionCatalog(
    const std::string& app_id,
    std::vector<ActionDefinition>& actions_out,
    std::string& error_message) const {
    const std::string catalog_path = JoinPath(JoinPath(JoinPath(repo_root_, "config"), "actions"), app_id + ".actions.json");
    std::string catalog_text;
    if (!ReadTextFile(catalog_path, catalog_text)) {
        error_message = "action catalog file not found: " + catalog_path;
        return false;
    }

    std::vector<CatalogAction> parsed_actions;
    if (!ParseActionsCatalog(catalog_text, parsed_actions, error_message)) {
        return false;
    }

    actions_out.clear();
    std::size_t index;
    for (index = 0; index < parsed_actions.size(); ++index) {
        ActionDefinition action;
        action.name = parsed_actions[index].name;
        action.label = parsed_actions[index].label;
        action.command = parsed_actions[index].command;
        action.cwd = parsed_actions[index].cwd;
        action.timeout_seconds = parsed_actions[index].timeout_seconds;
        action.detached = parsed_actions[index].detached;
        action.args_json = parsed_actions[index].args_json;
        actions_out.push_back(action);
    }
    return true;
}

bool ControlScriptRunner::ParseArgsObject(
    const std::string& args_json,
    std::map<std::string, std::string>& args_out,
    std::string& error_message) const {
    args_out.clear();
    const std::string trimmed = Trim(args_json.empty() ? "{}" : args_json);
    if (trimmed.size() < 2 || trimmed.front() != '{' || trimmed.back() != '}') {
        error_message = "args json must decode to an object";
        return false;
    }
    if (trimmed == "{}") {
        return true;
    }

    const std::regex pair_pattern("\\\"([^\\\"]+)\\\"\\s*:\\s*(\\\"(?:[^\\\"\\\\]|\\\\.)*\\\"|true|false|null|-?[0-9]+(?:\\.[0-9]+)?|\\{[^\\}]*\\}|\\[[^\\]]*\\])");
    std::sregex_iterator begin(trimmed.begin(), trimmed.end(), pair_pattern);
    std::sregex_iterator end;
    for (; begin != end; ++begin) {
        const std::string key = begin->str(1);
        const std::string raw_value = Trim(begin->str(2));
        if (!raw_value.empty() && raw_value.front() == '"' && raw_value.back() == '"') {
            args_out[key] = UnquoteJsonString(raw_value);
        } else if (raw_value == "null") {
            args_out[key] = "";
        } else {
            args_out[key] = raw_value;
        }
    }
    return true;
}

bool ControlScriptRunner::RunCatalogAction(
    const std::string& app_id,
    const std::string& action_name,
    const std::map<std::string, std::string>& args_map,
    double timeout_override_seconds,
    ActionRunResult& result_out,
    std::string& error_message) const {
    ActionRunResult result;
    result.ok = false;
    result.rc = 2;
    result.detached = false;
    result.pid = 0;
    result.payload_json = "{}";
    result.stdout_text.clear();
    result.stderr_text.clear();
    result.error_code.clear();
    result.error_message.clear();

    std::vector<ActionDefinition> actions;
    if (!LoadActionCatalog(app_id, actions, error_message)) {
        result.error_code = "actions_catalog_missing";
        result.error_message = error_message;
        result_out = result;
        return true;
    }

    const ActionDefinition* selected = NULL;
    std::size_t index;
    for (index = 0; index < actions.size(); ++index) {
        if (actions[index].name == action_name) {
            selected = &actions[index];
            break;
        }
    }
    if (selected == NULL) {
        result.error_code = "unknown_action";
        result.error_message = "unknown action: " + action_name;
        result_out = result;
        return true;
    }

    std::vector<std::string> command_parts;
    command_parts.reserve(selected->command.size());
    static const std::regex token_pattern("\\{([^{}]+)\\}");
    for (index = 0; index < selected->command.size(); ++index) {
        const std::string& token = selected->command[index];
        std::string rendered;
        std::string remaining = token;
        std::smatch match;
        while (std::regex_search(remaining, match, token_pattern)) {
            rendered.append(remaining.substr(0, static_cast<std::size_t>(match.position(0))));
            const std::string token_name = Trim(match[1].str());
            const std::map<std::string, std::string>::const_iterator iter = args_map.find(token_name);
            if (iter == args_map.end()) {
                result.error_code = "missing_action_arg";
                result.error_message = "missing action arg: " + token_name;
                result_out = result;
                return true;
            }
            rendered.append(iter->second);
            remaining = match.suffix().str();
        }
        rendered.append(remaining);
        command_parts.push_back(rendered);
    }

    std::string cwd = repo_root_;
    if (!selected->cwd.empty()) {
        if (PathLooksAbsolute(selected->cwd)) {
            const bool is_unc = selected->cwd.size() >= 2 && selected->cwd[0] == '\\' && selected->cwd[1] == '\\';
            cwd = is_unc ? repo_root_ : selected->cwd;
        } else {
            cwd = JoinPath(repo_root_, selected->cwd);
        }
        if (!PathExists(cwd)) {
            cwd = repo_root_;
        }
    }
    const std::string prefix = "pushd " + QuoteForCommandArg(cwd) + " && ";
    const std::string suffix = " && popd";
    const std::string command_line = BuildCommandLine(command_parts);

    if (selected->detached) {
        const std::string detached_text = prefix + "start \"\" " + command_line + " >nul 2>nul" + suffix;
        const int rc = std::system(detached_text.c_str());
        result.ok = rc == 0;
        result.rc = rc;
        result.detached = true;
        result.payload_json = "{\"detached\":true,\"action\":" + JsonQuote(action_name) + "}";
        if (!result.ok) {
            result.error_code = "action_failed";
            result.error_message = "detached action failed";
        }
        result_out = result;
        return true;
    }

    const double timeout_value = timeout_override_seconds > 0.0 ? timeout_override_seconds : selected->timeout_seconds;
    (void)timeout_value;

    const std::string full_command = prefix + command_line + " 2>&1" + suffix;
    std::string output;
    int exit_code = 0;
    if (!RunCommandCapture(full_command, output, exit_code, error_message)) {
        result.error_code = "action_launch_failed";
        result.error_message = error_message;
        result_out = result;
        return true;
    }

    result.stdout_text = output;
    result.rc = exit_code;
    std::string parsed_payload;
    if (TryExtractFirstJsonObject(output, parsed_payload)) {
        result.payload_json = CompactJson(parsed_payload);
    }
    if (exit_code == 0) {
        result.ok = true;
    } else {
        result.ok = false;
        result.error_code = "action_failed";
        result.error_message = "action failed";
        result.stderr_text = Trim(output);
    }

    result_out = result;
    return true;
}

std::string ControlScriptRunner::BuildActionInvokeResponse(const ActionRunResult& result) const {
    std::ostringstream stream;
    stream << "{";
    stream << "\"ok\":" << (result.ok ? "true" : "false") << ",";
    stream << "\"rc\":" << result.rc << ",";
    stream << "\"payload\":" << (IsJsonObjectText(result.payload_json) ? result.payload_json : "{}") << ",";
    stream << "\"stdout\":" << JsonQuote(result.stdout_text) << ",";
    stream << "\"stderr\":" << JsonQuote(result.stderr_text) << ",";
    if (result.ok) {
        stream << "\"error\":null";
    } else {
        stream << "\"error\":{";
        stream << "\"code\":" << JsonQuote(result.error_code.empty() ? "action_failed" : result.error_code) << ",";
        stream << "\"message\":" << JsonQuote(result.error_message.empty() ? "action failed" : result.error_message) << ",";
        stream << "\"details\":{}";
        stream << "}";
    }
    stream << "}";
    return stream.str();
}

bool ControlScriptRunner::RunConfigGet(
    const std::string& app_id,
    std::string& json_payload,
    std::string& error_message) const {
    ActionRunResult result;
    std::map<std::string, std::string> args_map;
    if (!RunCatalogAction(app_id, "config_show", args_map, 0.0, result, error_message)) {
        return false;
    }
    if (!result.ok) {
        error_message = result.error_message.empty() ? "config.get failed" : result.error_message;
        return false;
    }
    if (!IsJsonObjectText(result.payload_json)) {
        error_message = "config.get returned non-JSON payload";
        return false;
    }
    json_payload = result.payload_json;
    return true;
}

bool ControlScriptRunner::RunConfigSet(
    const std::string& app_id,
    const std::string& key,
    const std::string& value,
    std::string& json_payload,
    std::string& error_message) const {
    ActionRunResult result;
    std::map<std::string, std::string> args_map;
    args_map["key"] = key;
    args_map["value"] = value;
    if (!RunCatalogAction(app_id, "config_set_key", args_map, 0.0, result, error_message)) {
        return false;
    }
    if (!result.ok) {
        error_message = result.error_message.empty() ? "config.set failed" : result.error_message;
        return false;
    }
    if (IsJsonObjectText(result.payload_json) && result.payload_json != "{}") {
        json_payload = result.payload_json;
    } else {
        json_payload = BuildConfigSetFallbackPayload(key, value, result.rc, result.stdout_text);
    }
    return true;
}

bool ControlScriptRunner::RunActionList(
    const std::string& app_id,
    std::string& json_payload,
    std::string& error_message) const {
    std::vector<ActionDefinition> actions;
    if (!LoadActionCatalog(app_id, actions, error_message)) {
        return false;
    }

    std::ostringstream stream;
    stream << "{";
    stream << "\"actions\":[";
    std::size_t index;
    for (index = 0; index < actions.size(); ++index) {
        if (index > 0) {
            stream << ",";
        }
        stream << "{";
        stream << "\"name\":" << JsonQuote(actions[index].name) << ",";
        stream << "\"label\":" << JsonQuote(actions[index].label) << ",";
        stream << "\"args\":" << (actions[index].args_json.empty() ? "[]" : actions[index].args_json);
        stream << "}";
    }
    stream << "]";
    stream << "}";
    json_payload = stream.str();
    return true;
}

bool ControlScriptRunner::RunActionInvoke(
    const std::string& app_id,
    const std::string& action_name,
    const std::string& args_json,
    double timeout_seconds,
    std::string& json_payload,
    std::string& error_message) const {
    std::map<std::string, std::string> args_map;
    if (!ParseArgsObject(args_json, args_map, error_message)) {
        ActionRunResult bad_args;
        bad_args.ok = false;
        bad_args.rc = 2;
        bad_args.detached = false;
        bad_args.pid = 0;
        bad_args.payload_json = "{}";
        bad_args.stdout_text.clear();
        bad_args.stderr_text.clear();
        bad_args.error_code = "bad_args_json";
        bad_args.error_message = error_message;
        json_payload = BuildActionInvokeResponse(bad_args);
        error_message.clear();
        return true;
    }

    ActionRunResult result;
    if (!RunCatalogAction(app_id, action_name, args_map, timeout_seconds, result, error_message)) {
        return false;
    }
    json_payload = BuildActionInvokeResponse(result);
    error_message.clear();
    return true;
}

ControlScriptRunner CreateControlScriptRunner(const std::string& repo_root) {
    return ControlScriptRunner(repo_root, "", "");
}

}  // namespace Common
}  // namespace ProcessInterface
