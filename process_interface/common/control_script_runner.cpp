#include "control_script_runner.h"

#include <chrono>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <direct.h>
#include <fstream>
#include <sstream>
#include <string>

namespace ProcessInterface {
namespace Common {

namespace {

struct CommandArg {
    std::string name;
    std::string value;
};

bool LooksLikeJsonObject(const std::string& text) {
    std::size_t first;
    for (first = 0; first < text.size(); ++first) {
        const char c = text[first];
        if (c != ' ' && c != '\r' && c != '\n' && c != '\t') {
            break;
        }
    }
    if (first >= text.size()) {
        return false;
    }

    std::size_t last = text.size() - 1;
    while (last > first) {
        const char c = text[last];
        if (c != ' ' && c != '\r' && c != '\n' && c != '\t') {
            break;
        }
        --last;
    }
    return text[first] == '{' && text[last] == '}';
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

        if (c == '\r' || c == '\n' || c == '\t' || c == ' ') {
            continue;
        }
        output.push_back(c);
    }

    return output;
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

bool RunJsonCommand(
    const std::string& command_text,
    const std::string& command_name,
    std::string& json_payload,
    std::string& error_message) {
    FILE* process = _popen(command_text.c_str(), "r");
    if (process == NULL) {
        error_message = "failed to start " + command_name;
        return false;
    }

    char buffer[4096];
    std::ostringstream output_stream;
    while (fgets(buffer, sizeof(buffer), process) != NULL) {
        output_stream << buffer;
    }
    const int exit_code = _pclose(process);

    json_payload = output_stream.str();
    if (exit_code != 0) {
        error_message = command_name + " exited with code " + std::to_string(exit_code);
        return false;
    }
    if (!LooksLikeJsonObject(json_payload)) {
        error_message = command_name + " returned non-JSON output";
        return false;
    }

    json_payload = CompactJson(json_payload);
    return true;
}

std::string BuildCommandText(
    const std::string& script_path,
    const std::string& command_name,
    const CommandArg* args,
    std::size_t arg_count) {
    std::ostringstream command_stream;
    command_stream << "python " << QuoteForCommandArg(script_path) << " " << command_name;
    std::size_t index;
    for (index = 0; index < arg_count; ++index) {
        const CommandArg& arg = args[index];
        command_stream << " --" << arg.name << " " << QuoteForCommandArg(arg.value);
    }
    command_stream << " 2>&1";
    return command_stream.str();
}

}  // namespace

ControlScriptRunner::ControlScriptRunner(
    const std::string& repo_root,
    const std::string& script_relative_path,
    const std::string& script_label)
    : repo_root_(repo_root), script_relative_path_(script_relative_path), script_label_(script_label) {}

bool ControlScriptRunner::RunSimpleCommand(
    const std::string& command_name,
    const std::string& app_id,
    const std::string& key,
    const std::string& value,
    std::string& json_payload,
    std::string& error_message) const {
    const std::string script_path = repo_root_ + "\\" + script_relative_path_;
    CommandArg args[3];
    std::size_t arg_count = 0;

    args[arg_count].name = "app-id";
    args[arg_count].value = app_id;
    ++arg_count;

    if (!key.empty()) {
        args[arg_count].name = "key";
        args[arg_count].value = key;
        ++arg_count;
    }
    if (!value.empty()) {
        args[arg_count].name = "value";
        args[arg_count].value = value;
        ++arg_count;
    }

    const std::string command_text = BuildCommandText(script_path, command_name, args, arg_count);
    const std::string display_name = script_label_ + " " + command_name;
    return RunJsonCommand(command_text, display_name, json_payload, error_message);
}

bool ControlScriptRunner::WriteArgsJsonFile(
    const std::string& args_json,
    std::string& path_out,
    std::string& error_message) const {
    const std::string logs_dir = repo_root_ + "\\logs";
    const std::string process_interface_dir = logs_dir + "\\process-interface";
    const std::string temp_dir = process_interface_dir + "\\tmp";

    if (!EnsureDirectory(logs_dir) || !EnsureDirectory(process_interface_dir) || !EnsureDirectory(temp_dir)) {
        error_message = "failed to prepare temp directory for args-json";
        return false;
    }

    const long long now = static_cast<long long>(std::chrono::system_clock::now().time_since_epoch().count());
    const int random_value = std::rand();
    const std::string file_path =
        temp_dir + "\\invoke-args-" + std::to_string(now) + "-" + std::to_string(random_value) + ".json";
    std::ofstream output(file_path.c_str(), std::ios::binary);
    if (!output.is_open()) {
        error_message = "failed to open args-json temp file";
        return false;
    }
    output << args_json;
    output.close();
    path_out = file_path;
    return true;
}

bool ControlScriptRunner::RunConfigGet(
    const std::string& app_id,
    std::string& json_payload,
    std::string& error_message) const {
    return RunSimpleCommand("config-get", app_id, "", "", json_payload, error_message);
}

bool ControlScriptRunner::RunConfigSet(
    const std::string& app_id,
    const std::string& key,
    const std::string& value,
    std::string& json_payload,
    std::string& error_message) const {
    return RunSimpleCommand("config-set", app_id, key, value, json_payload, error_message);
}

bool ControlScriptRunner::RunActionList(
    const std::string& app_id,
    std::string& json_payload,
    std::string& error_message) const {
    return RunSimpleCommand("action-list", app_id, "", "", json_payload, error_message);
}

bool ControlScriptRunner::RunActionInvoke(
    const std::string& app_id,
    const std::string& action_name,
    const std::string& args_json,
    double timeout_seconds,
    std::string& json_payload,
    std::string& error_message) const {
    std::string args_json_file;
    if (!WriteArgsJsonFile(args_json, args_json_file, error_message)) {
        return false;
    }

    const std::string script_path = repo_root_ + "\\" + script_relative_path_;
    CommandArg args[4];
    args[0].name = "app-id";
    args[0].value = app_id;
    args[1].name = "action-name";
    args[1].value = action_name;
    args[2].name = "args-json-file";
    args[2].value = args_json_file;
    args[3].name = "timeout-seconds";
    args[3].value = std::to_string(timeout_seconds > 0.0 ? timeout_seconds : 30.0);

    const std::string command_text = BuildCommandText(script_path, "action-invoke", args, 4);
    const std::string display_name = script_label_ + " action-invoke";
    const bool ok = RunJsonCommand(command_text, display_name, json_payload, error_message);
    std::remove(args_json_file.c_str());
    return ok;
}

ControlScriptRunner CreateControlScriptRunner(const std::string& repo_root) {
    return ControlScriptRunner(repo_root, "scripts\\native_provider.py", "native_provider.py");
}

}  // namespace Common
}  // namespace ProcessInterface
