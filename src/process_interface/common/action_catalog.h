#ifndef PROCESS_INTERFACE_COMMON_ACTION_CATALOG_H
#define PROCESS_INTERFACE_COMMON_ACTION_CATALOG_H

#include <string>
#include <vector>

#include "../../common/fs_compat.h"
#include "../../common/path_templates.h"

namespace ProcessInterface {
namespace Common {

struct ActionDefinition {
    std::string name;
    std::string label;
    std::vector<std::string> command;
    std::string cwd;
    double timeout_seconds;
    bool detached;
    std::string args_json;
};

bool LoadActionCatalog(
    const fs::path& repo_root,
    const Common::PathTemplateSet& path_templates,
    const std::string& app_id,
    std::vector<ActionDefinition>& actions_out,
    std::string& error_message);

}  // namespace Common
}  // namespace ProcessInterface

#endif  // PROCESS_INTERFACE_COMMON_ACTION_CATALOG_H
