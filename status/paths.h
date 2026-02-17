#ifndef PROCESS_INTERFACE_STATUS_PATHS_H
#define PROCESS_INTERFACE_STATUS_PATHS_H

#include "fs.h"
#include <string>

namespace ProcessInterface {
namespace Status {

fs::path ResolveSpecPath(const fs::path& repo_root, const std::string& app_id);
fs::path ResolveSnapshotPath(const fs::path& repo_root, const std::string& app_id);

}  // namespace Status
}  // namespace ProcessInterface

#endif  // PROCESS_INTERFACE_STATUS_PATHS_H

