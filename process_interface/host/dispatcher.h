#ifndef PROCESS_INTERFACE_HOST_DISPATCHER_H
#define PROCESS_INTERFACE_HOST_DISPATCHER_H

#include <cstddef>
#include <string>

#include "../common/control_script_runner.h"
#include "../../wire_v0/wire_v0.h"

namespace ProcessInterface {
namespace Host {

struct HostContext {
    std::string repo_root;
    const std::string* allowed_app_ids;
    std::size_t allowed_app_count;
    Common::ControlScriptRunner control_runner;
};

struct RouteResult {
    bool ok;
    std::string response_json;
    std::string error_code;
    std::string error_message;
    std::string error_details_json;
};

RouteResult HandleRequest(const gpi::WireRequest& request, const HostContext& context);

}  // namespace Host
}  // namespace ProcessInterface

#endif  // PROCESS_INTERFACE_HOST_DISPATCHER_H
