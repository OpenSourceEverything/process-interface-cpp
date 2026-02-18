#ifndef PROCESS_INTERFACE_HOST_RUNTIME_HOST_PROFILE_H
#define PROCESS_INTERFACE_HOST_RUNTIME_HOST_PROFILE_H

#include <string>
#include <vector>

#include "../common/fs_compat.h"
#include "../common/path_templates.h"

namespace ProcessInterface {
namespace HostRuntime {

struct HostIpcProfile {
    std::string backend;
    std::string endpoint;
};

struct HostProfile {
    std::vector<std::string> allowed_apps;
    Common::PathTemplateSet path_templates;
    HostIpcProfile ipc;
};

bool LoadHostProfile(
    const Common::fs::path& profile_path,
    HostProfile& profile_out,
    std::string& error_message);

}  // namespace HostRuntime
}  // namespace ProcessInterface

#endif  // PROCESS_INTERFACE_HOST_RUNTIME_HOST_PROFILE_H
