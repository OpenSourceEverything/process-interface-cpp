#ifndef PROCESS_INTERFACE_COMMON_ACTION_JOBS_H
#define PROCESS_INTERFACE_COMMON_ACTION_JOBS_H

#include <string>

#include "../../common/fs_compat.h"
#include "../../common/path_templates.h"

namespace ProcessInterface {
namespace Common {

struct ActionJobRecord {
    std::string job_id;
    std::string state;
    std::string accepted_at;
    std::string started_at;
    std::string finished_at;
    std::string result_json;
    std::string stdout_text;
    std::string stderr_text;
    bool has_error;
    std::string error_code;
    std::string error_message;
};

std::string GenerateJobId();
fs::path ResolveActionJobPath(
    const fs::path& repo_root,
    const Common::PathTemplateSet& path_templates,
    const std::string& app_id,
    const std::string& job_id);

bool WriteActionJobRecord(
    const fs::path& repo_root,
    const Common::PathTemplateSet& path_templates,
    const std::string& app_id,
    const ActionJobRecord& record,
    std::string& error_message);

bool ReadActionJobRecord(
    const fs::path& repo_root,
    const Common::PathTemplateSet& path_templates,
    const std::string& app_id,
    const std::string& job_id,
    ActionJobRecord& record_out,
    std::string& error_message);

}  // namespace Common
}  // namespace ProcessInterface

#endif  // PROCESS_INTERFACE_COMMON_ACTION_JOBS_H
