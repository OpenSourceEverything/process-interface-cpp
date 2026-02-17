#include <iostream>
#include <string>

#include "../bridge_adapter/bridge_adapter.h"
#include "../status/api.h"
#include "../status/error_map.h"
#include "../wire_v0/wire_v0.h"

namespace {

std::string FindBridgeRepoRoot(int argc, char** argv) {
    int index;
    for (index = 1; index < argc; ++index) {
        const std::string token = argv[index];
        if (token == "--bridge-repo" && (index + 1) < argc) {
            return std::string(argv[index + 1]);
        }
    }
    return std::string();
}

std::string PingResponseJson() {
    return "{\"pong\":true,\"interfaceName\":\"generic-process-interface\",\"interfaceVersion\":1}";
}

}  // namespace

int main(int argc, char** argv) {
    const std::string bridge_repo_root = FindBridgeRepoRoot(argc, argv);
    if (bridge_repo_root.empty()) {
        std::cerr << "missing required arg: --bridge-repo" << std::endl;
        return 2;
    }

    gpi::BridgeStatusAdapter status_adapter(bridge_repo_root);
    std::string request_line;
    while (std::getline(std::cin, request_line)) {
        gpi::WireRequest request;
        std::string parse_error;
        if (!gpi::ParseRequestLine(request_line, request, parse_error)) {
            const std::string error_wire = gpi::BuildErrorResponse(
                request.request_id,
                "E_BAD_ARG",
                parse_error,
                "{}");
            std::cout << error_wire << std::endl;
            continue;
        }

        if (request.method == "ping") {
            const std::string ok_wire = gpi::BuildOkResponse(request.request_id, PingResponseJson());
            std::cout << ok_wire << std::endl;
            continue;
        }

        if (request.method == "status.get") {
            if (request.app_id.empty()) {
                const std::string error_wire = gpi::BuildErrorResponse(
                    request.request_id,
                    "E_BAD_ARG",
                    "missing required key: params.appId",
                    "{\"param\":\"appId\"}");
                std::cout << error_wire << std::endl;
                continue;
            }
            if (request.app_id != "bridge") {
                const std::string error_wire = gpi::BuildErrorResponse(
                    request.request_id,
                    "E_UNSUPPORTED_APP",
                    "unsupported appId",
                    "{\"appId\":\"" + gpi::JsonEscape(request.app_id) + "\"}");
                std::cout << error_wire << std::endl;
                continue;
            }

            const ProcessInterface::Status::StatusResult status_result =
                ProcessInterface::Status::CollectAndPublishStatus(bridge_repo_root, request.app_id);
            if (!status_result.ok) {
                const std::string error_wire = gpi::BuildErrorResponse(
                    request.request_id,
                    ProcessInterface::Status::ToIpcErrorCode(status_result.error_code),
                    status_result.error_message,
                    "{}");
                std::cout << error_wire << std::endl;
                continue;
            }

            const std::string ok_wire = gpi::BuildOkResponse(request.request_id, status_result.payload_json);
            std::cout << ok_wire << std::endl;
            continue;
        }

        if (request.method == "config.get") {
            if (request.app_id.empty()) {
                const std::string error_wire = gpi::BuildErrorResponse(
                    request.request_id,
                    "E_BAD_ARG",
                    "missing required key: params.appId",
                    "{\"param\":\"appId\"}");
                std::cout << error_wire << std::endl;
                continue;
            }
            if (request.app_id != "bridge") {
                const std::string error_wire = gpi::BuildErrorResponse(
                    request.request_id,
                    "E_UNSUPPORTED_APP",
                    "unsupported appId",
                    "{\"appId\":\"" + gpi::JsonEscape(request.app_id) + "\"}");
                std::cout << error_wire << std::endl;
                continue;
            }

            std::string config_json;
            std::string config_error;
            if (!status_adapter.GetConfigJson(request.app_id, config_json, config_error)) {
                const std::string error_wire = gpi::BuildErrorResponse(
                    request.request_id,
                    "E_INTERNAL",
                    config_error,
                    "{}");
                std::cout << error_wire << std::endl;
                continue;
            }

            const std::string ok_wire = gpi::BuildOkResponse(request.request_id, config_json);
            std::cout << ok_wire << std::endl;
            continue;
        }

        if (request.method == "config.set") {
            if (request.app_id.empty()) {
                const std::string error_wire = gpi::BuildErrorResponse(
                    request.request_id,
                    "E_BAD_ARG",
                    "missing required key: params.appId",
                    "{\"param\":\"appId\"}");
                std::cout << error_wire << std::endl;
                continue;
            }
            if (request.app_id != "bridge") {
                const std::string error_wire = gpi::BuildErrorResponse(
                    request.request_id,
                    "E_UNSUPPORTED_APP",
                    "unsupported appId",
                    "{\"appId\":\"" + gpi::JsonEscape(request.app_id) + "\"}");
                std::cout << error_wire << std::endl;
                continue;
            }
            if (request.key.empty()) {
                const std::string error_wire = gpi::BuildErrorResponse(
                    request.request_id,
                    "E_BAD_ARG",
                    "missing required key: params.key",
                    "{\"param\":\"key\"}");
                std::cout << error_wire << std::endl;
                continue;
            }

            std::string set_json;
            std::string set_error;
            if (!status_adapter.SetConfigValue(request.app_id, request.key, request.value, set_json, set_error)) {
                const std::string error_wire = gpi::BuildErrorResponse(
                    request.request_id,
                    "E_INTERNAL",
                    set_error,
                    "{}");
                std::cout << error_wire << std::endl;
                continue;
            }

            const std::string ok_wire = gpi::BuildOkResponse(request.request_id, set_json);
            std::cout << ok_wire << std::endl;
            continue;
        }

        if (request.method == "action.list") {
            if (request.app_id.empty()) {
                const std::string error_wire = gpi::BuildErrorResponse(
                    request.request_id,
                    "E_BAD_ARG",
                    "missing required key: params.appId",
                    "{\"param\":\"appId\"}");
                std::cout << error_wire << std::endl;
                continue;
            }
            if (request.app_id != "bridge") {
                const std::string error_wire = gpi::BuildErrorResponse(
                    request.request_id,
                    "E_UNSUPPORTED_APP",
                    "unsupported appId",
                    "{\"appId\":\"" + gpi::JsonEscape(request.app_id) + "\"}");
                std::cout << error_wire << std::endl;
                continue;
            }

            std::string actions_json;
            std::string actions_error;
            if (!status_adapter.ListActionsJson(request.app_id, actions_json, actions_error)) {
                const std::string error_wire = gpi::BuildErrorResponse(
                    request.request_id,
                    "E_INTERNAL",
                    actions_error,
                    "{}");
                std::cout << error_wire << std::endl;
                continue;
            }

            const std::string ok_wire = gpi::BuildOkResponse(request.request_id, actions_json);
            std::cout << ok_wire << std::endl;
            continue;
        }

        if (request.method == "action.invoke") {
            if (request.app_id.empty()) {
                const std::string error_wire = gpi::BuildErrorResponse(
                    request.request_id,
                    "E_BAD_ARG",
                    "missing required key: params.appId",
                    "{\"param\":\"appId\"}");
                std::cout << error_wire << std::endl;
                continue;
            }
            if (request.app_id != "bridge") {
                const std::string error_wire = gpi::BuildErrorResponse(
                    request.request_id,
                    "E_UNSUPPORTED_APP",
                    "unsupported appId",
                    "{\"appId\":\"" + gpi::JsonEscape(request.app_id) + "\"}");
                std::cout << error_wire << std::endl;
                continue;
            }
            if (request.action_name.empty()) {
                const std::string error_wire = gpi::BuildErrorResponse(
                    request.request_id,
                    "E_BAD_ARG",
                    "missing required key: params.actionName",
                    "{\"param\":\"actionName\"}");
                std::cout << error_wire << std::endl;
                continue;
            }

            std::string invoke_json;
            std::string invoke_error;
            if (!status_adapter.InvokeAction(
                    request.app_id,
                    request.action_name,
                    request.args_json,
                    request.timeout_seconds,
                    invoke_json,
                    invoke_error)) {
                const std::string error_wire = gpi::BuildErrorResponse(
                    request.request_id,
                    "E_INTERNAL",
                    invoke_error,
                    "{}");
                std::cout << error_wire << std::endl;
                continue;
            }

            const std::string ok_wire = gpi::BuildOkResponse(request.request_id, invoke_json);
            std::cout << ok_wire << std::endl;
            continue;
        }

        const std::string unsupported_wire = gpi::BuildErrorResponse(
            request.request_id,
            "E_UNSUPPORTED_METHOD",
            "unsupported method: " + request.method,
            "{\"method\":\"" + gpi::JsonEscape(request.method) + "\"}");
        std::cout << unsupported_wire << std::endl;
    }

    return 0;
}
