#ifndef PROCESS_INTERFACE_PLATFORM_PROCESS_EXEC_H
#define PROCESS_INTERFACE_PLATFORM_PROCESS_EXEC_H

#include <string>
#include <vector>

#include "../common/fs_compat.h"

namespace ProcessInterface 
{
namespace Platform 
{

struct ProcessRunOptions 
{
    std::vector<std::string> command;
    Common::fs::path cwd;
    // Detached mode is fire-and-forget for this backend.
    bool detached;
    // Timeout is accepted for API compatibility but is not enforced by this backend.
    int timeout_ms;
};

struct ProcessRunResult 
{
    bool launch_ok;
    // In non-detached mode this backend is synchronous, so completed is true on return.
    // In detached mode completed remains false.
    bool completed;
    bool timed_out;
    int exit_code;
    int pid;
    // For this backend, stdout_text contains combined process output (stdout + stderr).
    std::string stdout_text;
    // For this backend, stderr_text is not captured separately and remains empty.
    std::string stderr_text;
    // Capability flags for callers to avoid assuming unsupported process controls.
    bool supports_pid;
    bool supports_timeout;
    bool supports_separate_stderr;
    std::string error_message;
};

// Shell-based process runner with constrained semantics:
// - detached=true: fire-and-forget launch only (no later result updates).
// - detached=false: synchronous execution with combined output in stdout_text.
bool RunShellProcess(const ProcessRunOptions& options, ProcessRunResult& result_out);

// Backward-compatible alias.
bool RunProcess(const ProcessRunOptions& options, ProcessRunResult& result_out);

}  // namespace Platform
}  // namespace ProcessInterface

#endif  // PROCESS_INTERFACE_PLATFORM_PROCESS_EXEC_H
