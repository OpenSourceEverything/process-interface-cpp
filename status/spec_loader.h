#ifndef PROCESS_INTERFACE_STATUS_SPEC_LOADER_H
#define PROCESS_INTERFACE_STATUS_SPEC_LOADER_H

#include "fs.h"
#include <string>
#include <vector>

#include "error_map.h"

namespace ProcessInterface {
namespace Status {

struct StatusSpec {
    std::string app_id;
    std::string app_title;
    std::string running_field;
    std::string pid_field;
    std::string host_running_field;
    std::string host_pid_field;
    std::vector<std::string> operations;
};

StatusErrorCode LoadStatusSpec(
    const fs::path& repo_root,
    const std::string& app_id,
    StatusSpec& spec_out,
    std::string& error_message);

}  // namespace Status
}  // namespace ProcessInterface

#endif  // PROCESS_INTERFACE_STATUS_SPEC_LOADER_H

