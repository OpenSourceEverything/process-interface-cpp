#include "action_executor.h"

#include <regex>
#include <sstream>

#include "../../../external/nlohmann/json.hpp"
#include "../../common/text.h"
#include "../../platform/process_exec.h"

namespace ProcessInterface {
namespace Common {

namespace {

bool TryExtractFirstJsonObject(const std::string& text, std::string& object_json_out) {
    std::size_t start = 0;
    while (start < text.size() && text[start] != '{') {
        ++start;
    }
    if (start >= text.size()) {
        return false;
    }

    bool in_string = false;
    bool escaped = false;
    int depth = 0;

    std::size_t index;
    for (index = start; index < text.size(); ++index) {
        const char c = text[index];

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

        if (c == '{') {
            ++depth;
            continue;
        }

        if (c == '}') {
            --depth;
            if (depth == 0) {
                object_json_out = text.substr(start, index - start + 1);
                return true;
            }
        }
    }

    return false;
}

std::string CompactObjectJson(const std::string& raw_json) {
    try {
        const nlohmann::json value = nlohmann::json::parse(raw_json);
        if (value.is_object()) {
            return value.dump();
        }
    } catch (const std::exception&) {
    }
    return std::string("{}");
}

fs::path ResolveActionCwd(const fs::path& repo_root, const ActionDefinition& action) {
    if (action.cwd.empty()) {
        return repo_root;
    }

    if (action.cwd.size() >= 2 && action.cwd[0] == '\\' && action.cwd[1] == '\\') {
        return repo_root;
    }

    const fs::path configured(action.cwd);
    if (configured.is_absolute()) {
        if (fs::exists(configured)) {
            return configured;
        }
        return repo_root;
    }

    const fs::path combined = repo_root / configured;
    if (fs::exists(combined)) {
        return combined;
    }
    return repo_root;
}

bool RenderCommand(
    const ActionDefinition& action,
    const std::map<std::string, std::string>& args_map,
    std::vector<std::string>& command_out,
    std::string& missing_arg_name) {
    command_out.clear();
    command_out.reserve(action.command.size());

    static const std::regex token_pattern("\\{([^{}]+)\\}");

    std::size_t index;
    for (index = 0; index < action.command.size(); ++index) {
        const std::string& token = action.command[index];

        std::string rendered;
        std::string remaining = token;
        std::smatch match;

        while (std::regex_search(remaining, match, token_pattern)) {
            rendered.append(remaining.substr(0, static_cast<std::size_t>(match.position(0))));

            const std::string token_name = TrimCopy(match[1].str());
            const std::map<std::string, std::string>::const_iterator iter = args_map.find(token_name);
            if (iter == args_map.end()) {
                missing_arg_name = token_name;
                return false;
            }
            rendered.append(iter->second);

            remaining = match.suffix().str();
        }

        rendered.append(remaining);
        command_out.push_back(rendered);
    }

    return true;
}

bool IsPythonToken(const std::string& token) {
    const std::string lowered = TrimCopy(token);
    if (lowered.empty()) {
        return false;
    }

    std::string text;
    text.reserve(lowered.size());
    std::size_t index;
    for (index = 0; index < lowered.size(); ++index) {
        const char c = lowered[index];
        if (c >= 'A' && c <= 'Z') {
            text.push_back(static_cast<char>(c - 'A' + 'a'));
        } else {
            text.push_back(c);
        }
    }

    if (text == "python" || text == "python.exe") {
        return true;
    }

    const std::size_t pos = text.find("python");
    return pos != std::string::npos;
}

void ApplyPythonScriptFallback(std::vector<std::string>& command_parts, const fs::path& cwd) {
    if (command_parts.size() < 2) {
        return;
    }
    if (!IsPythonToken(command_parts[0])) {
        return;
    }

    const std::string entry = TrimCopy(command_parts[1]);
    if (entry.empty() || entry[0] == '-') {
        return;
    }

    const fs::path raw_entry(entry);
    if (raw_entry.has_extension()) {
        return;
    }
    if (entry.find('/') != std::string::npos || entry.find('\\') != std::string::npos) {
        return;
    }

    const fs::path direct = cwd / raw_entry;
    if (fs::exists(direct)) {
        return;
    }

    const fs::path direct_py = cwd / (entry + ".py");
    if (fs::exists(direct_py)) {
        command_parts[1] = direct_py.string();
        return;
    }

    const fs::path ops_py = cwd / "ops" / "scripts" / (entry + ".py");
    if (fs::exists(ops_py)) {
        command_parts[1] = ops_py.string();
        return;
    }

    const fs::path scripts_py = cwd / "scripts" / (entry + ".py");
    if (fs::exists(scripts_py)) {
        command_parts[1] = scripts_py.string();
    }
}

}  // namespace

bool ExecuteCatalogAction(
    const fs::path& repo_root,
    const std::vector<ActionDefinition>& actions,
    const std::string& action_name,
    const std::map<std::string, std::string>& args_map,
    double timeout_override_seconds,
    ActionRunResult& result_out,
    std::string& error_message) {
    ActionRunResult result;
    result.ok = false;
    result.rc = 2;
    result.detached = false;
    result.pid = 0;
    result.timed_out = false;
    result.payload_json = "{}";
    result.stdout_text.clear();
    result.stderr_text.clear();
    result.error_code.clear();
    result.error_message.clear();

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

    std::vector<std::string> rendered_command;
    std::string missing_arg_name;
    if (!RenderCommand(*selected, args_map, rendered_command, missing_arg_name)) {
        result.error_code = "missing_action_arg";
        result.error_message = "missing action arg: " + missing_arg_name;
        result_out = result;
        return true;
    }

    const fs::path action_cwd = ResolveActionCwd(repo_root, *selected);
    ApplyPythonScriptFallback(rendered_command, action_cwd);
    const double timeout_seconds = timeout_override_seconds > 0.0 ? timeout_override_seconds : selected->timeout_seconds;
    const int timeout_ms = timeout_seconds > 0.0
        ? static_cast<int>(timeout_seconds * 1000.0)
        : 30000;

    Platform::ProcessRunOptions run_options;
    run_options.command = rendered_command;
    run_options.cwd = action_cwd;
    run_options.detached = selected->detached;
    run_options.timeout_ms = timeout_ms;

    Platform::ProcessRunResult process_result;
    if (!Platform::RunProcess(run_options, process_result)) {
        result.error_code = "action_launch_failed";
        result.error_message = process_result.error_message.empty() ? "action launch failed" : process_result.error_message;
        result_out = result;
        return true;
    }

    result.detached = selected->detached;
    result.pid = process_result.pid;
    result.stdout_text = process_result.stdout_text;
    result.stderr_text = process_result.stderr_text;
    result.timed_out = process_result.timed_out;

    if (selected->detached) {
        nlohmann::json payload;
        payload["detached"] = true;
        payload["pid"] = process_result.pid > 0 ? nlohmann::json(process_result.pid) : nlohmann::json(nullptr);
        payload["action"] = action_name;

        result.ok = process_result.launch_ok;
        result.rc = 0;
        result.payload_json = payload.dump();
        if (!result.ok) {
            result.error_code = "action_failed";
            result.error_message = "detached action failed";
        }

        result_out = result;
        return true;
    }

    result.rc = process_result.exit_code;

    std::string parsed_payload;
    if (TryExtractFirstJsonObject(process_result.stdout_text, parsed_payload)) {
        result.payload_json = CompactObjectJson(parsed_payload);
    }

    if (process_result.timed_out) {
        result.ok = false;
        result.error_code = "action_timeout";
        result.error_message = "action timed out";
        result_out = result;
        return true;
    }

    if (process_result.exit_code == 0) {
        result.ok = true;
    } else {
        result.ok = false;
        result.error_code = "action_failed";
        result.error_message = "action failed";
    }

    result_out = result;
    return true;
}

}  // namespace Common
}  // namespace ProcessInterface
