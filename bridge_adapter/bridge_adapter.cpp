#include "bridge_adapter.h"

#include <chrono>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <direct.h>
#include <fstream>
#include <sstream>
#include <string>

namespace gpi {

namespace {

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

std::string BuildConfigGetCommand(const std::string& bridge_repo_root) {
    std::ostringstream command_stream;
    command_stream << "python \""
                   << bridge_repo_root
                   << "\\scripts\\native_provider_bridge.py\" config-get --app-id bridge 2>&1";
    return command_stream.str();
}

std::string BuildConfigSetCommand(
    const std::string& bridge_repo_root,
    const std::string& key,
    const std::string& value) {
    std::ostringstream command_stream;
    command_stream << "python \""
                   << bridge_repo_root
                   << "\\scripts\\native_provider_bridge.py\" config-set --app-id bridge --key "
                   << QuoteForCommandArg(key)
                   << " --value "
                   << QuoteForCommandArg(value)
                   << " 2>&1";
    return command_stream.str();
}

std::string BuildActionListCommand(const std::string& bridge_repo_root) {
    std::ostringstream command_stream;
    command_stream << "python \""
                   << bridge_repo_root
                   << "\\scripts\\native_provider_bridge.py\" action-list --app-id bridge 2>&1";
    return command_stream.str();
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

bool EnsureTempDirectory(const std::string& bridge_repo_root, std::string& temp_dir_out) {
    const std::string logs_dir = bridge_repo_root + "\\logs";
    const std::string process_interface_dir = logs_dir + "\\process-interface";
    const std::string temp_dir = process_interface_dir + "\\tmp";

    if (!EnsureDirectory(logs_dir)) {
        return false;
    }
    if (!EnsureDirectory(process_interface_dir)) {
        return false;
    }
    if (!EnsureDirectory(temp_dir)) {
        return false;
    }
    temp_dir_out = temp_dir;
    return true;
}

bool WriteArgsJsonFile(const std::string& bridge_repo_root, const std::string& args_json, std::string& path_out) {
    std::string temp_dir;
    if (!EnsureTempDirectory(bridge_repo_root, temp_dir)) {
        return false;
    }

    const auto now = std::chrono::system_clock::now().time_since_epoch().count();
    const int random_value = std::rand();
    const std::string file_path =
        temp_dir + "\\invoke-args-" + std::to_string(now) + "-" + std::to_string(random_value) + ".json";
    std::ofstream output(file_path.c_str(), std::ios::binary);
    if (!output.is_open()) {
        return false;
    }
    output << args_json;
    output.close();
    path_out = file_path;
    return true;
}

std::string BuildActionInvokeCommand(
    const std::string& bridge_repo_root,
    const std::string& action_name,
    const std::string& args_json_file,
    double timeout_seconds) {
    std::ostringstream command_stream;
    command_stream << "python \""
                   << bridge_repo_root
                   << "\\scripts\\native_provider_bridge.py\" action-invoke --app-id bridge --action-name "
                   << QuoteForCommandArg(action_name)
                   << " --args-json-file "
                   << QuoteForCommandArg(args_json_file)
                   << " --timeout-seconds "
                   << timeout_seconds
                   << " 2>&1";
    return command_stream.str();
}

}  // namespace

BridgeStatusAdapter::BridgeStatusAdapter(const std::string& bridge_repo_root) : bridge_repo_root_(bridge_repo_root) {}

bool BridgeStatusAdapter::ReadStatusJson(const std::string& app_id, std::string& status_json, std::string& error_message) {
    (void)status_json;
    if (app_id != "bridge") {
        error_message = "unsupported appId";
        return false;
    }
    error_message = "status.get is handled by ProcessInterface::Status in host runtime";
    return false;
}

bool BridgeStatusAdapter::GetConfigJson(const std::string& app_id, std::string& config_json, std::string& error_message) {
    if (app_id != "bridge") {
        error_message = "unsupported appId";
        return false;
    }

    const std::string command_text = BuildConfigGetCommand(bridge_repo_root_);
    return RunJsonCommand(command_text, "native_provider_bridge.py config-get", config_json, error_message);
}

bool BridgeStatusAdapter::SetConfigValue(
    const std::string& app_id,
    const std::string& key,
    const std::string& value,
    std::string& result_json,
    std::string& error_message) {
    if (app_id != "bridge") {
        error_message = "unsupported appId";
        return false;
    }

    const std::string command_text = BuildConfigSetCommand(bridge_repo_root_, key, value);
    return RunJsonCommand(command_text, "native_provider_bridge.py config-set", result_json, error_message);
}

bool BridgeStatusAdapter::ListActionsJson(const std::string& app_id, std::string& actions_json, std::string& error_message) {
    if (app_id != "bridge") {
        error_message = "unsupported appId";
        return false;
    }

    const std::string command_text = BuildActionListCommand(bridge_repo_root_);
    return RunJsonCommand(command_text, "native_provider_bridge.py action-list", actions_json, error_message);
}

bool BridgeStatusAdapter::InvokeAction(
    const std::string& app_id,
    const std::string& action_name,
    const std::string& args_json,
    double timeout_seconds,
    std::string& result_json,
    std::string& error_message) {
    if (app_id != "bridge") {
        error_message = "unsupported appId";
        return false;
    }

    std::string args_json_file;
    if (!WriteArgsJsonFile(bridge_repo_root_, args_json, args_json_file)) {
        error_message = "failed to create args-json temp file";
        return false;
    }

    const std::string command_text =
        BuildActionInvokeCommand(bridge_repo_root_, action_name, args_json_file, timeout_seconds > 0.0 ? timeout_seconds : 30.0);
    const bool ok = RunJsonCommand(command_text, "native_provider_bridge.py action-invoke", result_json, error_message);
    std::remove(args_json_file.c_str());
    return ok;
}

}  // namespace gpi
