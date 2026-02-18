#ifndef PROCESS_INTERFACE_STATUS_API_H
#define PROCESS_INTERFACE_STATUS_API_H

#include "fs.h"
#include <string>

#include "../common/path_templates.h"
#include "error_map.h"

namespace ProcessInterface {
namespace Status {

struct StatusResult {
    bool ok;
    StatusErrorCode error_code;
    std::string payload_json;
    std::string error_message;
};

StatusResult CollectAndPublishStatus(
    const fs::path& repo_root,
    const std::string& app_id,
    const Common::PathTemplateSet& path_templates);

}  // namespace Status
}  // namespace ProcessInterface

#endif  // PROCESS_INTERFACE_STATUS_API_H

