#ifndef PROCESS_INTERFACE_COMMON_ACTION_EXECUTOR_H
#define PROCESS_INTERFACE_COMMON_ACTION_EXECUTOR_H

#include <map>
#include <string>
#include <vector>

#include "action_catalog.h"

namespace ProcessInterface {
namespace Common {

struct ActionRunResult {
    bool ok;
    int rc;
    bool detached;
    int pid;
    bool timed_out;
    std::string payload_json;
    std::string stdout_text;
    std::string stderr_text;
    std::string error_code;
    std::string error_message;
};

bool ExecuteCatalogAction(
    const fs::path& repo_root,
    const std::vector<ActionDefinition>& actions,
    const std::string& action_name,
    const std::map<std::string, std::string>& args_map,
    double timeout_override_seconds,
    ActionRunResult& result_out,
    std::string& error_message);

}  // namespace Common
}  // namespace ProcessInterface

#endif  // PROCESS_INTERFACE_COMMON_ACTION_EXECUTOR_H
