#include <iostream>
#include <memory>
#include <string>

#include "../ipc/factory/IpcFactory.h"

namespace {

struct ClientArgs {
    std::string backend;
    std::string endpoint;
    std::string request_json;
};

bool ParseArgs(int argc, char** argv, ClientArgs& args_out, std::string& error_message) {
    ClientArgs args;
    args.backend = "zmq";

    int index = 0;
    for (index = 1; index < argc; ++index) {
        const std::string token = argv[index];
        if (token == "--backend") {
            if ((index + 1) >= argc) {
                error_message = "missing value for --backend";
                return false;
            }
            args.backend = argv[++index];
            continue;
        }
        if (token == "--ipc-endpoint") {
            if ((index + 1) >= argc) {
                error_message = "missing value for --ipc-endpoint";
                return false;
            }
            args.endpoint = argv[++index];
            continue;
        }
        if (token == "--request-json") {
            if ((index + 1) >= argc) {
                error_message = "missing value for --request-json";
                return false;
            }
            args.request_json = argv[++index];
            continue;
        }

        error_message = "unsupported arg: " + token;
        return false;
    }

    if (args.endpoint.empty()) {
        error_message = "missing required arg: --ipc-endpoint";
        return false;
    }
    if (args.request_json.empty()) {
        error_message = "missing required arg: --request-json";
        return false;
    }

    args_out = args;
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    ClientArgs args;
    std::string parse_error;
    if (!ParseArgs(argc, argv, args, parse_error)) {
        std::cerr << parse_error << std::endl;
        return 2;
    }

    std::string factory_error;
    std::unique_ptr<ProcessInterface::Ipc::IIpcClient> client =
        ProcessInterface::Ipc::CreateIpcClient(args.backend, factory_error);
    if (!client) {
        std::cerr << factory_error << std::endl;
        return 2;
    }

    std::string connect_error;
    if (!client->Connect(args.endpoint, connect_error)) {
        std::cerr << connect_error << std::endl;
        return 2;
    }

    std::string response_json;
    std::string request_error;
    if (!client->Request(args.request_json, response_json, request_error)) {
        std::cerr << request_error << std::endl;
        return 2;
    }

    std::cout << response_json << std::endl;
    return 0;
}
