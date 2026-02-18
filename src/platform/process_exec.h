#ifndef PROCESS_INTERFACE_PLATFORM_PROCESS_EXEC_H
#define PROCESS_INTERFACE_PLATFORM_PROCESS_EXEC_H

#include <string>
#include <vector>

#include "../common/fs_compat.h"

namespace ProcessInterface {
namespace Platform {

struct ProcessRunOptions {
    std::vector<std::string> command;
    Common::fs::path cwd;
    bool detached;
    int timeout_ms;
};

struct ProcessRunResult {
    bool launch_ok;
    bool completed;
    bool timed_out;
    int exit_code;
    int pid;
    std::string stdout_text;
    std::string stderr_text;
    std::string error_message;
};

bool RunProcess(const ProcessRunOptions& options, ProcessRunResult& result_out);

}  // namespace Platform
}  // namespace ProcessInterface

#endif  // PROCESS_INTERFACE_PLATFORM_PROCESS_EXEC_H
