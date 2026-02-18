#ifndef PROCESS_INTERFACE_COMMON_PATH_TEMPLATES_H
#define PROCESS_INTERFACE_COMMON_PATH_TEMPLATES_H

#include <string>

#include "fs_compat.h"

namespace ProcessInterface {
namespace Common {

struct PathTemplateSet {
    std::string status_spec_path;
    std::string status_snapshot_path;
    std::string action_catalog_path;
    std::string action_job_path;
};

struct PathTemplateArgs {
    std::string repo_root;
    std::string app_id;
    std::string job_id;
};

bool ValidateTemplateHasToken(
    const std::string& template_text,
    const std::string& token_name,
    std::string& error_message);

std::string RenderTemplate(const std::string& template_text, const PathTemplateArgs& args);
fs::path RenderTemplatePath(const std::string& template_text, const PathTemplateArgs& args);

}  // namespace Common
}  // namespace ProcessInterface

#endif  // PROCESS_INTERFACE_COMMON_PATH_TEMPLATES_H
