#ifndef PROCESS_INTERFACE_COMMON_ACTION_RESPONSE_H
#define PROCESS_INTERFACE_COMMON_ACTION_RESPONSE_H

#include <string>

#include "action_jobs.h"

namespace ProcessInterface {
namespace Common {

std::string BuildConfigSetFallbackPayload(
    const std::string& key,
    const std::string& value,
    int rc,
    const std::string& stdout_text);

std::string BuildActionInvokeAcceptedResponse(const std::string& job_id, const std::string& accepted_at);
std::string BuildActionJobResponse(const ActionJobRecord& record);

}  // namespace Common
}  // namespace ProcessInterface

#endif  // PROCESS_INTERFACE_COMMON_ACTION_RESPONSE_H
