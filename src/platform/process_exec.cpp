#include "process_exec.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

namespace ProcessInterface {
namespace Platform {

namespace {

namespace fs = ProcessInterface::Common::fs;

std::string QuoteForShell(const std::string& value) {
    std::string quoted = "\"";
    std::size_t index = 0;
    for (index = 0; index < value.size(); ++index) {
        const char c = value[index];
        if (c == '"') {
            quoted += "\"\"";
        } else {
            quoted.push_back(c);
        }
    }
    quoted += "\"";
    return quoted;
}

bool NeedsQuoting(const std::string& value) {
    std::size_t index = 0;
    for (index = 0; index < value.size(); ++index) {
        const char c = value[index];
        if (c == ' ' || c == '\t' || c == '"') {
            return true;
        }
    }
    return false;
}

std::string BuildShellCommand(const std::vector<std::string>& command_parts) {
    std::string command;
    std::size_t index = 0;
    for (index = 0; index < command_parts.size(); ++index) {
        if (index > 0) {
            command += " ";
        }

        const std::string& token = command_parts[index];
        if (!NeedsQuoting(token)) {
            command += token;
            continue;
        }

        if (index == 0) {
            // cmd.exe parsing requires an extra leading quote for quoted executable paths.
            command += "\"";
            command += QuoteForShell(token);
            continue;
        }

        command += QuoteForShell(token);
    }
    command += " 2>&1";
    return command;
}

int NormalizeExitCode(int raw_exit_code) {
    if (raw_exit_code < 0) {
        return -1;
    }
    return raw_exit_code;
}

}  // namespace

bool RunProcess(const ProcessRunOptions& options, ProcessRunResult& result_out) {
    ProcessRunResult result;
    result.launch_ok = false;
    result.completed = false;
    result.timed_out = false;
    result.exit_code = -1;
    result.pid = 0;

    if (options.command.empty()) {
        result.error_message = "command cannot be empty";
        result_out = result;
        return false;
    }

    const fs::path original_cwd = fs::current_path();
    if (!options.cwd.empty()) {
        std::error_code cwd_ec;
        fs::current_path(options.cwd, cwd_ec);
        if (cwd_ec) {
            result.error_message = "failed to set process cwd: " + options.cwd.string();
            result_out = result;
            return false;
        }
    }

    const std::string shell_command = BuildShellCommand(options.command);
    result.launch_ok = true;

    if (options.detached) {
        std::thread([shell_command]() {
            std::system(shell_command.c_str());
        }).detach();
        fs::current_path(original_cwd);
        result_out = result;
        return true;
    }

    FILE* pipe = ::popen(shell_command.c_str(), "r");
    if (pipe == NULL) {
        fs::current_path(original_cwd);
        result.error_message = "popen failed";
        result_out = result;
        return false;
    }

    char buffer[4096];
    while (std::fgets(buffer, static_cast<int>(sizeof(buffer)), pipe) != NULL) {
        result.stdout_text += buffer;
    }

    const int close_rc = ::pclose(pipe);
    result.exit_code = NormalizeExitCode(close_rc);
    result.completed = true;
    result.stderr_text = result.stdout_text;

    fs::current_path(original_cwd);
    result_out = result;
    return true;
}

}  // namespace Platform
}  // namespace ProcessInterface
