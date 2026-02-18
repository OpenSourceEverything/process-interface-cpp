#include <iostream>
#include <string>

#include "../process_interface/common/control_script_runner.h"
#include "../process_interface/host/dispatcher.h"
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

const std::string kAllowedApps[] = {"bridge"};

}  // namespace

int main(int argc, char** argv) {
    const std::string bridge_repo_root = FindBridgeRepoRoot(argc, argv);
    if (bridge_repo_root.empty()) {
        std::cerr << "missing required arg: --bridge-repo" << std::endl;
        return 2;
    }

    ProcessInterface::Host::HostContext host_context = {
        bridge_repo_root,
        kAllowedApps,
        sizeof(kAllowedApps) / sizeof(kAllowedApps[0]),
        ProcessInterface::Common::CreateControlScriptRunner(bridge_repo_root),
    };

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

        const ProcessInterface::Host::RouteResult route_result =
            ProcessInterface::Host::HandleRequest(request, host_context);
        if (route_result.ok) {
            const std::string ok_wire = gpi::BuildOkResponse(request.request_id, route_result.response_json);
            std::cout << ok_wire << std::endl;
            continue;
        }

        const std::string error_wire = gpi::BuildErrorResponse(
            request.request_id,
            route_result.error_code,
            route_result.error_message,
            route_result.error_details_json);
        std::cout << error_wire << std::endl;
    }

    return 0;
}
