#include "process_exec.h"

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <thread>

#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
#define PROCESS_INTERFACE_PLATFORM_WINDOWS 1
#else
#define PROCESS_INTERFACE_PLATFORM_WINDOWS 0
#endif

#if !PROCESS_INTERFACE_PLATFORM_WINDOWS
#include <sys/wait.h>
#endif

namespace ProcessInterface {
namespace Platform {

namespace {

namespace fs = ProcessInterface::Common::fs;

std::string QuoteForShellWindows(const std::string& value) {
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

bool NeedsQuotingWindows(const std::string& value) {
    std::size_t index = 0;
    for (index = 0; index < value.size(); ++index) {
        const char c = value[index];
        if (c == ' ' || c == '\t' || c == '"') {
            return true;
        }
    }
    return false;
}

bool IsSafePosixShellChar(const char c) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
        return true;
    }
    return c == '_' || c == '-' || c == '.' || c == '/' || c == ':' || c == '+' || c == ',' || c == '=';
}

std::string QuoteForShellPosix(const std::string& value) {
    std::string quoted = "'";
    std::size_t index = 0;
    for (index = 0; index < value.size(); ++index) {
        const char c = value[index];
        if (c == '\'') {
            quoted += "'\\''";
        } else {
            quoted.push_back(c);
        }
    }
    quoted += "'";
    return quoted;
}

bool NeedsQuotingPosix(const std::string& value) {
    if (value.empty()) {
        return true;
    }

    std::size_t index = 0;
    for (index = 0; index < value.size(); ++index) {
        if (!IsSafePosixShellChar(value[index])) {
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
#if PROCESS_INTERFACE_PLATFORM_WINDOWS
        if (!NeedsQuotingWindows(token)) {
            command += token;
            continue;
        }

        if (index == 0) {
            // cmd.exe parsing requires an extra leading quote for quoted executable paths.
            command += "\"";
            command += QuoteForShellWindows(token);
            continue;
        }

        command += QuoteForShellWindows(token);
#else
        if (!NeedsQuotingPosix(token)) {
            command += token;
            continue;
        }
        command += QuoteForShellPosix(token);
#endif
    }
    command += " 2>&1";
    return command;
}

FILE* OpenReadPipe(const char* command) {
#if PROCESS_INTERFACE_PLATFORM_WINDOWS
    return ::_popen(command, "r");
#else
    return ::popen(command, "r");
#endif
}

int ClosePipe(FILE* pipe) {
#if PROCESS_INTERFACE_PLATFORM_WINDOWS
    return ::_pclose(pipe);
#else
    return ::pclose(pipe);
#endif
}

int DecodeExitCode(const int raw_status) {
#if PROCESS_INTERFACE_PLATFORM_WINDOWS
    if (raw_status < 0) {
        return -1;
    }
    return raw_status;
#else
    if (raw_status == -1) {
        return -1;
    }
    if (WIFEXITED(raw_status)) {
        return WEXITSTATUS(raw_status);
    }
    if (WIFSIGNALED(raw_status)) {
        return 128 + WTERMSIG(raw_status);
    }
    return -1;
#endif
}

class ScopedCwdRestore {
public:
    explicit ScopedCwdRestore(const fs::path& original_cwd)
        : original_cwd_(original_cwd),
          active_(true) {}

    ~ScopedCwdRestore() {
        if (!active_) {
            return;
        }

        std::error_code restore_ec;
        fs::current_path(original_cwd_, restore_ec);
    }

    ScopedCwdRestore(const ScopedCwdRestore&) = delete;
    ScopedCwdRestore& operator=(const ScopedCwdRestore&) = delete;

private:
    fs::path original_cwd_;
    bool active_;
};

}  // namespace

bool RunShellProcess(const ProcessRunOptions& options, ProcessRunResult& result_out) {
    ProcessRunResult result;
    result.launch_ok = false;
    result.completed = false;
    // This shell-based implementation does not support enforced timeouts.
    result.timed_out = false;
    result.exit_code = -1;
    // PID is not available from system()/popen() here.
    result.pid = 0;
    result.stdout_text.clear();
    // stdout/stderr are merged by "2>&1", so stderr is intentionally left empty.
    result.stderr_text.clear();
    result.supports_pid = false;
    result.supports_timeout = false;
    result.supports_separate_stderr = false;
    result.error_message.clear();

    (void)options.timeout_ms;

    if (options.command.empty()) {
        result.error_message = "command cannot be empty";
        result_out = result;
        return false;
    }

    std::error_code capture_ec;
    const fs::path original_cwd = fs::current_path(capture_ec);
    if (capture_ec) {
        result.error_message = "failed to get process cwd";
        result_out = result;
        return false;
    }
    ScopedCwdRestore restore_cwd(original_cwd);

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
        try {
            std::thread([shell_command]() {
                const int system_rc = std::system(shell_command.c_str());
                (void)system_rc;
            }).detach();
        } catch (const std::exception& ex) {
            result.launch_ok = false;
            result.error_message = std::string("failed to start detached runner: ") + ex.what();
            result_out = result;
            return false;
        } catch (...) {
            result.launch_ok = false;
            result.error_message = "failed to start detached runner";
            result_out = result;
            return false;
        }

        result_out = result;
        return true;
    }

    FILE* pipe = OpenReadPipe(shell_command.c_str());
    if (pipe == NULL) {
        result.launch_ok = false;
        result.error_message = "popen failed";
        result_out = result;
        return false;
    }

    char buffer[4096];
    while (std::fgets(buffer, static_cast<int>(sizeof(buffer)), pipe) != NULL) {
        result.stdout_text += buffer;
    }

    const int close_rc = ClosePipe(pipe);
    result.exit_code = DecodeExitCode(close_rc);
    result.completed = true;

    result_out = result;
    return true;
}

bool RunProcess(const ProcessRunOptions& options, ProcessRunResult& result_out) {
    return RunShellProcess(options, result_out);
}

}  // namespace Platform
}  // namespace ProcessInterface
