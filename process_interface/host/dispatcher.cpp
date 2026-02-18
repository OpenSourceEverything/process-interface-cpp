#include "dispatcher.h"

#include <string>

#include "../action/service.h"
#include "../config/service.h"
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
    std::size_t index;
    for (index = 0; index < context.allowed_app_count; ++index) {
        if (context.allowed_app_ids[index] == app_id) {
            return true;
        }
    }
    return false;
}

RouteResult ValidateAppId(const HostContext& context, const gpi::WireRequest& request) {
    if (request.app_id.empty()) {
        return MakeError(kBadArg, "missing required key: params.appId", "{\"param\":\"appId\"}");
    }
    if (!IsAllowedApp(context, request.app_id)) {
        return MakeError(
            kUnsupportedApp,
            "unsupported appId",
            "{\"appId\":\"" + gpi::JsonEscape(request.app_id) + "\"}");
    }
    return MakeOk("{}");
}

}  // namespace

RouteResult HandleRequest(const gpi::WireRequest& request, const HostContext& context) {
    if (request.method == "ping") {
        return MakeOk("{\"pong\":true,\"interfaceName\":\"generic-process-interface\",\"interfaceVersion\":1}");
    }

    if (request.method == "status.get") {
        const RouteResult app_check = ValidateAppId(context, request);
        if (!app_check.ok) {
            return app_check;
        }

        const ProcessInterface::Status::StatusResult status_result =
            ProcessInterface::Status::CollectAndPublishStatus(context.repo_root, request.app_id);
        if (!status_result.ok) {
            return MakeError(
                ProcessInterface::Status::ToIpcErrorCode(status_result.error_code),
                status_result.error_message,
                "{}");
        }
        return MakeOk(status_result.payload_json);
    }

    if (request.method == "config.get") {
        const RouteResult app_check = ValidateAppId(context, request);
        if (!app_check.ok) {
            return app_check;
        }

        std::string response_json;
        std::string error_message;
        if (!ProcessInterface::Config::GetConfigJson(
                context.control_runner,
                request.app_id,
                response_json,
                error_message)) {
            return MakeError(kInternal, error_message.empty() ? "config.get failed" : error_message, "{}");
        }
        return MakeOk(response_json);
    }

    if (request.method == "config.set") {
        const RouteResult app_check = ValidateAppId(context, request);
        if (!app_check.ok) {
            return app_check;
        }
        if (request.key.empty()) {
            return MakeError(kBadArg, "missing required key: params.key", "{\"param\":\"key\"}");
        }

        std::string response_json;
        std::string error_message;
        if (!ProcessInterface::Config::SetConfigValue(
                context.control_runner,
                request.app_id,
                request.key,
                request.value,
                response_json,
                error_message)) {
            return MakeError(kInternal, error_message.empty() ? "config.set failed" : error_message, "{}");
        }
        return MakeOk(response_json);
    }

    if (request.method == "action.list") {
        const RouteResult app_check = ValidateAppId(context, request);
        if (!app_check.ok) {
            return app_check;
        }

        std::string response_json;
        std::string error_message;
        if (!ProcessInterface::Action::ListActionsJson(
                context.control_runner,
                request.app_id,
                response_json,
                error_message)) {
            return MakeError(kInternal, error_message.empty() ? "action.list failed" : error_message, "{}");
        }
        return MakeOk(response_json);
    }

    if (request.method == "action.invoke") {
        const RouteResult app_check = ValidateAppId(context, request);
        if (!app_check.ok) {
            return app_check;
        }
        if (request.action_name.empty()) {
            return MakeError(kBadArg, "missing required key: params.actionName", "{\"param\":\"actionName\"}");
        }

        std::string response_json;
        std::string error_message;
        if (!ProcessInterface::Action::InvokeAction(
                context.control_runner,
                request.app_id,
                request.action_name,
                request.args_json.empty() ? "{}" : request.args_json,
                request.timeout_seconds,
                response_json,
                error_message)) {
            return MakeError(kInternal, error_message.empty() ? "action.invoke failed" : error_message, "{}");
        }
        return MakeOk(response_json);
    }

    return MakeError(
        kUnsupportedMethod,
        "unsupported method: " + request.method,
        "{\"method\":\"" + gpi::JsonEscape(request.method) + "\"}");
}

}  // namespace Host
}  // namespace ProcessInterface
