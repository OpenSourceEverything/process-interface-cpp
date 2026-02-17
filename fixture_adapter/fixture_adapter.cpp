#include "fixture_adapter.h"

#include <cstdio>
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

bool IsSupportedApp(const std::string& app_id) {
    return app_id == "40318" || app_id == "plc-simulator" || app_id == "ble-simulator";
}

std::string BuildStatusCommand(const std::string& fixture_repo_root) {
    std::ostringstream command_stream;
    command_stream << "python \""
                   << fixture_repo_root
                   << "\\scripts\\fixture_status.py\" --json --bridge-timeout-seconds 0.75 2>&1";
    return command_stream.str();
}

}  // namespace

FixtureStatusAdapter::FixtureStatusAdapter(const std::string& fixture_repo_root)
    : fixture_repo_root_(fixture_repo_root) {}

bool FixtureStatusAdapter::ReadStatusJson(const std::string& app_id, std::string& status_json, std::string& error_message) {
    if (!IsSupportedApp(app_id)) {
        error_message = "unsupported appId";
        return false;
    }

    const std::string command_text = BuildStatusCommand(fixture_repo_root_);
    FILE* process = _popen(command_text.c_str(), "r");
    if (process == NULL) {
        error_message = "failed to start fixture_status.py";
        return false;
    }

    char buffer[4096];
    std::ostringstream output_stream;
    while (fgets(buffer, sizeof(buffer), process) != NULL) {
        output_stream << buffer;
    }
    const int exit_code = _pclose(process);

    status_json = output_stream.str();
    if (exit_code != 0) {
        error_message = "fixture_status.py exited with code " + std::to_string(exit_code);
        return false;
    }
    if (!LooksLikeJsonObject(status_json)) {
        error_message = "fixture_status.py returned non-JSON output";
        return false;
    }
    status_json = CompactJson(status_json);

    return true;
}

}  // namespace gpi
