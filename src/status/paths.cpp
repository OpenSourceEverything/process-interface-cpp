#include "paths.h"

namespace ProcessInterface {
namespace Status {

fs::path ResolveSpecPath(
    const fs::path& repo_root,
    const Common::PathTemplateSet& path_templates,
    const std::string& app_id) {
    const Common::PathTemplateArgs path_args = {
        repo_root.string(),
        app_id,
        std::string(),
    };
    return Common::RenderTemplatePath(path_templates.status_spec_path, path_args);
}

fs::path ResolveSnapshotPath(
    const fs::path& repo_root,
    const Common::PathTemplateSet& path_templates,
    const std::string& app_id) {
    const Common::PathTemplateArgs path_args = {
        repo_root.string(),
        app_id,
        std::string(),
    };
    return Common::RenderTemplatePath(path_templates.status_snapshot_path, path_args);
}

}  // namespace Status
}  // namespace ProcessInterface

