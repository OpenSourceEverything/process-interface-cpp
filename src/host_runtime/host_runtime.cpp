#include "host_runtime.h"

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "host_profile.h"
#include "../common/fs_compat.h"
#include "../ipc/factory/IpcFactory.h"
#include "../process_interface/common/control_script_runner.h"
#include "../process_interface/host/dispatcher.h"
#include "../wire_v0/wire_v0.h"

namespace ProcessInterface {
namespace HostRuntime {

namespace {

struct LaunchArgs {
    std::string repo_root;
    std::string host_config_path;
    std::string ipc_endpoint_override;
};

bool ParseLaunchArgs(int argc, char** argv, LaunchArgs& args_out, std::string& error_message) {
    LaunchArgs args;

    int index = 0;
    for (index = 1; index < argc; ++index) {
        const std::string token = argv[index];
        if (token == "--repo") {
            if ((index + 1) >= argc) {
                error_message = "missing value for --repo";
                return false;
            }
            args.repo_root = argv[++index];
            continue;
        }
        if (token == "--host-config") {
            if ((index + 1) >= argc) {
                error_message = "missing value for --host-config";
                return false;
            }
            args.host_config_path = argv[++index];
            continue;
        }
        if (token == "--ipc-endpoint") {
            if ((index + 1) >= argc) {
                error_message = "missing value for --ipc-endpoint";
                return false;
            }
            args.ipc_endpoint_override = argv[++index];
            continue;
        }
        error_message = "unsupported arg: " + token;
        return false;
    }

    if (args.repo_root.empty()) {
        error_message = "missing required arg: --repo";
        return false;
    }
    if (args.host_config_path.empty()) {
        error_message = "missing required arg: --host-config";
        return false;
    }

    args_out = args;
    return true;
}

}  // namespace

int RunHost(int argc, char** argv) {
    LaunchArgs launch_args;
    std::string launch_error;
    if (!ParseLaunchArgs(argc, argv, launch_args, launch_error)) {
        std::cerr << launch_error << std::endl;
        return 2;
    }

    HostProfile profile;
    std::string profile_error;
    if (!LoadHostProfile(launch_args.host_config_path, profile, profile_error)) {
        std::cerr << profile_error << std::endl;
        return 2;
    }

    const std::string endpoint =
        launch_args.ipc_endpoint_override.empty() ? profile.ipc.endpoint : launch_args.ipc_endpoint_override;

    ProcessInterface::Host::HostContext host_context = {
        launch_args.repo_root,
        profile.allowed_apps,
        profile.path_templates,
        ProcessInterface::Common::CreateControlScriptRunner(launch_args.repo_root, profile.path_templates),
    };

    std::string factory_error;
    std::unique_ptr<ProcessInterface::Ipc::IIpcServer> ipc_server =
        ProcessInterface::Ipc::CreateIpcServer(profile.ipc.backend, factory_error);
    if (!ipc_server) {
        std::cerr << factory_error << std::endl;
        return 2;
    }

    std::string bind_error;
    if (!ipc_server->Bind(endpoint, bind_error)) {
        std::cerr << bind_error << std::endl;
        return 2;
    }

    ipc_server->SetRequestHandler(
        [&](const std::string& request_payload) -> std::string {
            gpi::WireRequest request;
            std::string parse_error;
            if (!gpi::ParseRequestLine(request_payload, request, parse_error)) {
                return gpi::BuildErrorResponse(
                    request.request_id,
                    "E_BAD_ARG",
                    parse_error,
                    "{}");
            }

            const ProcessInterface::Host::RouteResult route_result =
                ProcessInterface::Host::HandleRequest(request, host_context);

            if (route_result.ok) {
                return gpi::BuildOkResponse(request.request_id, route_result.response_json);
            }

            return gpi::BuildErrorResponse(
                request.request_id,
                route_result.error_code,
                route_result.error_message,
                route_result.error_details_json);
        });

    std::string run_error;
    if (!ipc_server->Run(run_error)) {
        std::cerr << run_error << std::endl;
        return 2;
    }

    return 0;
}

}  // namespace HostRuntime
}  // namespace ProcessInterface
