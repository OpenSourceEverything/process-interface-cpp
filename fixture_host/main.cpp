#include <iostream>
#include <string>

#include "../fixture_adapter/fixture_adapter.h"
#include "../wire_v0/wire_v0.h"

namespace {

std::string FindFixtureRepoRoot(int argc, char** argv) {
    int index;
    for (index = 1; index < argc; ++index) {
        const std::string token = argv[index];
        if (token == "--fixture-repo" && (index + 1) < argc) {
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
    const std::string fixture_repo_root = FindFixtureRepoRoot(argc, argv);
    if (fixture_repo_root.empty()) {
        std::cerr << "missing required arg: --fixture-repo" << std::endl;
        return 2;
    }

    gpi::FixtureStatusAdapter status_adapter(fixture_repo_root);
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

            std::string status_json;
            std::string status_error;
            if (!status_adapter.ReadStatusJson(request.app_id, status_json, status_error)) {
                const std::string error_wire = gpi::BuildErrorResponse(
                    request.request_id,
                    "E_INTERNAL",
                    status_error,
                    "{}");
                std::cout << error_wire << std::endl;
                continue;
            }

            const std::string ok_wire = gpi::BuildOkResponse(request.request_id, status_json);
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
