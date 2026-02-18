#include "action_response.h"

#include "../../../external/nlohmann/json.hpp"
#include "../../common/text.h"

namespace ProcessInterface {
namespace Common {

std::string BuildConfigSetFallbackPayload(
    const std::string& key,
    const std::string& value,
    int rc,
    const std::string& stdout_text) {
    nlohmann::json response;
    response["ok"] = rc == 0;
    response["key"] = key;
    response["value"] = value;

    nlohmann::json output = nlohmann::json::array();
    const std::vector<std::string> lines = SplitNonEmptyLines(stdout_text);
    std::size_t index;
    for (index = 0; index < lines.size(); ++index) {
        output.push_back(lines[index]);
    }

    response["output"] = output;
    return response.dump();
}

std::string BuildActionInvokeAcceptedResponse(const std::string& job_id, const std::string& accepted_at) {
    nlohmann::json response;
    response["jobId"] = job_id;
    response["state"] = "queued";
    response["acceptedAt"] = accepted_at;
    return response.dump();
}

std::string BuildActionJobResponse(const ActionJobRecord& record) {
    nlohmann::json response;
    response["jobId"] = record.job_id;
    response["state"] = record.state;
    response["acceptedAt"] = record.accepted_at;
    response["startedAt"] = record.started_at;
    response["finishedAt"] = record.finished_at;

    try {
        response["result"] = nlohmann::json::parse(record.result_json.empty() ? "{}" : record.result_json);
    } catch (const std::exception&) {
        response["result"] = nlohmann::json::object();
    }

    response["stdout"] = record.stdout_text;
    response["stderr"] = record.stderr_text;

    if (record.has_error) {
        nlohmann::json error;
        error["code"] = record.error_code;
        error["message"] = record.error_message;
        error["details"] = nlohmann::json::object();
        response["error"] = error;
    } else {
        response["error"] = nullptr;
    }

    return response.dump();
}

}  // namespace Common
}  // namespace ProcessInterface
