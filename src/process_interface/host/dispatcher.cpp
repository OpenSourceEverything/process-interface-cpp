#include "dispatcher.h"

#include <string>
#include <unordered_map>
#include <vector>

#include "../../status/api.h"
#include "../../status/error_map.h"
#include "../../wire_v0/wire_v0.h"

namespace ProcessInterface {
namespace Host {

namespace {

const char* kBadArg = "E_BAD_ARG";
const char* kUnsupportedApp = "E_UNSUPPORTED_APP";
const char* kUnsupportedMethod = "E_UNSUPPORTED_METHOD";
const char* kInternal = "E_INTERNAL";
const char* kNotFound = "E_NOT_FOUND";

enum class ParamKey {
    kAppId,
    kKey,
    kActionName,
    kJobId,
};

typedef RouteResult (*MethodHandler)(const gpi::WireRequest&, const HostContext&);

struct MethodSpec {
    const char* method;
    std::vector<ParamKey> required_params;
    MethodHandler handler;
};

RouteResult MakeOk(const std::string& response_json) {
    RouteResult result;
    result.ok = true;
    result.response_json = response_json;
    return result;
}

RouteResult MakeError(
    const std::string& error_code,
    const std::string& error_message,
    const std::string& error_details_json) {
    RouteResult result;
    result.ok = false;
    result.error_code = error_code;
    result.error_message = error_message;
    result.error_details_json = error_details_json;
    return result;
}

bool IsAllowedApp(const HostContext& context, const std::string& app_id) {
    std::size_t index = 0;
    for (index = 0; index < context.allowed_app_ids.size(); ++index) {
        if (context.allowed_app_ids[index] == app_id) {
            return true;
        }
    }
    return false;
}

RouteResult ValidateRequiredParam(const gpi::WireRequest& request, ParamKey param) {
    if (param == ParamKey::kAppId && request.app_id.empty()) {
        return MakeError(kBadArg, "missing required key: params.appId", "{\"param\":\"appId\"}");
    }
    if (param == ParamKey::kKey && request.key.empty()) {
        return MakeError(kBadArg, "missing required key: params.key", "{\"param\":\"key\"}");
    }
    if (param == ParamKey::kActionName && request.action_name.empty()) {
        return MakeError(kBadArg, "missing required key: params.actionName", "{\"param\":\"actionName\"}");
    }
    if (param == ParamKey::kJobId && request.job_id.empty()) {
        return MakeError(kBadArg, "missing required key: params.jobId", "{\"param\":\"jobId\"}");
    }
    return MakeOk("{}");
}

RouteResult ValidateMethodSpec(
    const MethodSpec& spec,
    const gpi::WireRequest& request,
    const HostContext& context) {
    std::size_t index = 0;
    for (index = 0; index < spec.required_params.size(); ++index) {
        const RouteResult param_check = ValidateRequiredParam(request, spec.required_params[index]);
        if (!param_check.ok) {
            return param_check;
        }
    }

    const bool requires_app = request.method != "ping";
    if (requires_app && !IsAllowedApp(context, request.app_id)) {
        return MakeError(
            kUnsupportedApp,
            "unsupported appId",
            "{\"appId\":\"" + gpi::JsonEscape(request.app_id) + "\"}");
    }

    return MakeOk("{}");
}

RouteResult HandlePing(const gpi::WireRequest&, const HostContext&) {
    return MakeOk("{\"pong\":true,\"interfaceName\":\"generic-process-interface\",\"interfaceVersion\":1}");
}

RouteResult HandleStatusGet(const gpi::WireRequest& request, const HostContext& context) {
    const ProcessInterface::Status::StatusResult status_result =
        ProcessInterface::Status::CollectAndPublishStatus(context.repo_root, request.app_id, context.path_templates);
    if (!status_result.ok) {
        return MakeError(
            ProcessInterface::Status::ToIpcErrorCode(status_result.error_code),
            status_result.error_message,
            "{}");
    }
    return MakeOk(status_result.payload_json);
}

RouteResult HandleConfigGet(const gpi::WireRequest& request, const HostContext& context) {
    std::string response_json;
    std::string error_message;
    if (!context.control_runner.RunConfigGet(request.app_id, response_json, error_message)) {
        return MakeError(kInternal, error_message.empty() ? "config.get failed" : error_message, "{}");
    }
    return MakeOk(response_json);
}

RouteResult HandleConfigSet(const gpi::WireRequest& request, const HostContext& context) {
    std::string response_json;
    std::string error_message;
    if (!context.control_runner.RunConfigSet(request.app_id, request.key, request.value, response_json, error_message)) {
        return MakeError(kInternal, error_message.empty() ? "config.set failed" : error_message, "{}");
    }
    return MakeOk(response_json);
}

RouteResult HandleActionList(const gpi::WireRequest& request, const HostContext& context) {
    std::string response_json;
    std::string error_message;
    if (!context.control_runner.RunActionList(request.app_id, response_json, error_message)) {
        return MakeError(kInternal, error_message.empty() ? "action.list failed" : error_message, "{}");
    }
    return MakeOk(response_json);
}

RouteResult HandleActionInvoke(const gpi::WireRequest& request, const HostContext& context) {
    std::string response_json;
    std::string error_message;
    if (!context.control_runner.RunActionInvoke(
            request.app_id,
            request.action_name,
            request.args_json.empty() ? "{}" : request.args_json,
            request.timeout_seconds,
            response_json,
            error_message)) {
        if (error_message.find("bad args:") == 0) {
            return MakeError(kBadArg, error_message.substr(9), "{\"param\":\"args\"}");
        }
        return MakeError(kInternal, error_message.empty() ? "action.invoke failed" : error_message, "{}");
    }
    return MakeOk(response_json);
}

RouteResult HandleActionJobGet(const gpi::WireRequest& request, const HostContext& context) {
    std::string response_json;
    std::string error_message;
    if (!context.control_runner.RunActionJobGet(request.app_id, request.job_id, response_json, error_message)) {
        if (error_message == "job not found") {
            return MakeError(kNotFound, "job not found", "{\"jobId\":\"" + gpi::JsonEscape(request.job_id) + "\"}");
        }
        return MakeError(kInternal, error_message.empty() ? "action.job.get failed" : error_message, "{}");
    }
    return MakeOk(response_json);
}

const MethodSpec kMethodSpecs[] = {
    {"ping", {}, &HandlePing},
    {"status.get", {ParamKey::kAppId}, &HandleStatusGet},
    {"config.get", {ParamKey::kAppId}, &HandleConfigGet},
    {"config.set", {ParamKey::kAppId, ParamKey::kKey}, &HandleConfigSet},
    {"action.list", {ParamKey::kAppId}, &HandleActionList},
    {"action.invoke", {ParamKey::kAppId, ParamKey::kActionName}, &HandleActionInvoke},
    {"action.job.get", {ParamKey::kAppId, ParamKey::kJobId}, &HandleActionJobGet},
};

const std::unordered_map<std::string, MethodSpec> kMethodMap = {
    {"ping", kMethodSpecs[0]},
    {"status.get", kMethodSpecs[1]},
    {"config.get", kMethodSpecs[2]},
    {"config.set", kMethodSpecs[3]},
    {"action.list", kMethodSpecs[4]},
    {"action.invoke", kMethodSpecs[5]},
    {"action.job.get", kMethodSpecs[6]},
};

}  // namespace

RouteResult HandleRequest(const gpi::WireRequest& request, const HostContext& context) {
    const std::unordered_map<std::string, MethodSpec>::const_iterator iter = kMethodMap.find(request.method);
    if (iter == kMethodMap.end()) {
        return MakeError(
            kUnsupportedMethod,
            "unsupported method: " + request.method,
            "{\"method\":\"" + gpi::JsonEscape(request.method) + "\"}");
    }

    const MethodSpec& spec = iter->second;
    const RouteResult validation = ValidateMethodSpec(spec, request, context);
    if (!validation.ok) {
        return validation;
    }

    return spec.handler(request, context);
}

}  // namespace Host
}  // namespace ProcessInterface
