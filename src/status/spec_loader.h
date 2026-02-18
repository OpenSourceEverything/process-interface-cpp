#ifndef PROCESS_INTERFACE_STATUS_SPEC_LOADER_H
#define PROCESS_INTERFACE_STATUS_SPEC_LOADER_H

#include "fs.h"
#include <string>
#include <vector>

#include "../common/path_templates.h"
#include "error_map.h"
#include "status_expression_parser.h"

namespace ProcessInterface {
namespace Status {

struct StatusSpec {
    std::string app_id;
    std::string app_title;
    std::string running_field;
    std::string pid_field;
    std::string host_running_field;
    std::string host_pid_field;
    std::vector<ParsedOperation> operations;
};

StatusErrorCode LoadStatusSpec(
    const fs::path& repo_root,
    const Common::PathTemplateSet& path_templates,
    const std::string& app_id,
    StatusSpec& spec_out,
    std::string& error_message);

}  // namespace Status
}  // namespace ProcessInterface

#endif  // PROCESS_INTERFACE_STATUS_SPEC_LOADER_H

