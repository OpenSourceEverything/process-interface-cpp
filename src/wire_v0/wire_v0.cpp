#include "wire_v0.h"

#include <string>

#include "../../external/nlohmann/json.hpp"

namespace gpi {

namespace {

nlohmann::json ParseObjectOrDefault(const std::string& text, const nlohmann::json& default_value) {
    try {
        const nlohmann::json value = nlohmann::json::parse(text);
        if (value.is_object()) {
            return value;
        }
    } catch (const std::exception&) {
    }
    return default_value;
}

}  // namespace

std::string JsonEscape(const std::string& value) {
    const std::string quoted = nlohmann::json(value).dump();
    if (quoted.size() < 2) {
        return std::string();
    }
    return quoted.substr(1, quoted.size() - 2);
}

bool ParseRequestLine(const std::string& request_line, WireRequest& request, std::string& error_message) {
    nlohmann::json root;
    try {
        root = nlohmann::json::parse(request_line);
    } catch (const std::exception&) {
        error_message = "request is not a JSON object";
        return false;
    }

    if (!root.is_object()) {
        error_message = "request is not a JSON object";
        return false;
    }

    request = WireRequest();
    request.args_json = "{}";
    request.timeout_seconds = 0.0;

    if (root.contains("id") && root["id"].is_string()) {
        request.request_id = root["id"].get<std::string>();
    }

    if (!root.contains("method") || !root["method"].is_string()) {
        error_message = "missing required key: method";
        return false;
    }
    request.method = root["method"].get<std::string>();

    nlohmann::json params = nlohmann::json::object();
    if (root.contains("params")) {
        params = root["params"];
        if (!params.is_object()) {
            error_message = "params must be a JSON object";
            return false;
        }
    }

    if (params.contains("appId") && params["appId"].is_string()) {
        request.app_id = params["appId"].get<std::string>();
    }

    if (params.contains("key") && params["key"].is_string()) {
        request.key = params["key"].get<std::string>();
    }

    if (params.contains("value")) {
        if (params["value"].is_string()) {
            request.value = params["value"].get<std::string>();
        } else {
            request.value = params["value"].dump();
        }
    }

    if (params.contains("actionName") && params["actionName"].is_string()) {
        request.action_name = params["actionName"].get<std::string>();
    }

    if (params.contains("jobId") && params["jobId"].is_string()) {
        request.job_id = params["jobId"].get<std::string>();
    }

    if (params.contains("args")) {
        if (!params["args"].is_object()) {
            error_message = "params.args must be a JSON object";
            return false;
        }
        request.args_json = params["args"].dump();
    }

    if (params.contains("timeoutSeconds") && params["timeoutSeconds"].is_number()) {
        const double timeout_seconds = params["timeoutSeconds"].get<double>();
        if (timeout_seconds > 0.0) {
            request.timeout_seconds = timeout_seconds;
        }
    }

    return true;
}

std::string BuildOkResponse(const std::string& request_id, const std::string& response_json_object) 
{
    nlohmann::json payload = ParseObjectOrDefault(response_json_object, nlohmann::json::object());

    nlohmann::json response;
    if (!request_id.empty()) {
        response["id"] = request_id;
    }
    response["ok"] = true;
    response["response"] = payload;
    return response.dump();
}

std::string BuildErrorResponse(const std::string& request_id,
                               const std::string& error_code,
                               const std::string& error_message,
                               const std::string& error_details_json_object) 
{
    nlohmann::json details = ParseObjectOrDefault(error_details_json_object, nlohmann::json::object());

    nlohmann::json response;
    if (!request_id.empty()) {
        response["id"] = request_id;
    }

    response["ok"] = false;
    response["error"] = {
        {"code", error_code},
        {"message", error_message},
        {"details", details},
    };
    return response.dump();
}

}  // namespace gpi
