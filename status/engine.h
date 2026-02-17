#ifndef PROCESS_INTERFACE_STATUS_ENGINE_H
#define PROCESS_INTERFACE_STATUS_ENGINE_H

#include <string>

#include "context.h"
#include "error_map.h"
#include "spec_loader.h"

namespace ProcessInterface {
namespace Status {

StatusErrorCode ExecuteStatusSpec(
    const StatusSpec& spec,
    const StatusContext& context,
    std::string& payload_json,
    std::string& error_message);

}  // namespace Status
}  // namespace ProcessInterface

#endif  // PROCESS_INTERFACE_STATUS_ENGINE_H
