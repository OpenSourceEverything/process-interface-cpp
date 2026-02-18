#ifndef PROCESS_INTERFACE_STATUS_OPERATION_REGISTRY_H
#define PROCESS_INTERFACE_STATUS_OPERATION_REGISTRY_H

#include <map>
#include <string>

#include "../../external/nlohmann/json.hpp"
#include "context.h"
#include "error_map.h"
#include "status_expression_parser.h"

namespace ProcessInterface {
namespace Status {

StatusErrorCode EvaluateOperation(
    const ParsedOperation& operation,
    const std::map<std::string, nlohmann::json>& values,
    const StatusContext& context,
    nlohmann::json& out_json,
    std::string& error_message);

}  // namespace Status
}  // namespace ProcessInterface

#endif  // PROCESS_INTERFACE_STATUS_OPERATION_REGISTRY_H
