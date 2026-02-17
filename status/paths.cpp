#include "paths.h"

namespace ProcessInterface {
namespace Status {

fs::path ResolveSpecPath(const fs::path& repo_root, const std::string& app_id) {
    return repo_root / "config" / "process-interface" / "status" / (app_id + ".status.json");
}

fs::path ResolveSnapshotPath(const fs::path& repo_root, const std::string& app_id) {
    return repo_root / "logs" / "process-interface" / "status-source" / (app_id + ".json");
}

}  // namespace Status
}  // namespace ProcessInterface

