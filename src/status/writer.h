#ifndef PROCESS_INTERFACE_STATUS_WRITER_H
#define PROCESS_INTERFACE_STATUS_WRITER_H

#include "fs.h"
#include <string>

#include "../common/path_templates.h"
#include "error_map.h"

namespace ProcessInterface {
namespace Status {

StatusErrorCode WriteSnapshotEnvelope(
    const fs::path& repo_root,
    const Common::PathTemplateSet& path_templates,
    const std::string& app_id,
    const std::string& payload_json,
    std::string& error_message);

}  // namespace Status
}  // namespace ProcessInterface

#endif  // PROCESS_INTERFACE_STATUS_WRITER_H

