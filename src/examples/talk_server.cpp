#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include "../ipc/factory/IpcFactory.h"
#include "../../external/nlohmann/json.hpp"

namespace {

struct ServerArgs {
    std::string backend;
    std::string endpoint;
    int max_requests;
};

bool ParsePositiveInt(const std::string& value, int& out_value) {
    char* end = NULL;
    const long parsed = std::strtol(value.c_str(), &end, 10);
    if (end == value.c_str() || *end != '\0') {
        return false;
    }
    if (parsed <= 0 || parsed > 100000) {
        return false;
    }
    out_value = static_cast<int>(parsed);
    return true;
}

bool ParseArgs(int argc, char** argv, ServerArgs& args_out, std::string& error_message) {
    ServerArgs args;
    args.backend = "zmq";
    args.endpoint = "tcp://127.0.0.1:5580";
    args.max_requests = 4;

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
        if (token == "--endpoint") {
            if ((index + 1) >= argc) {
                error_message = "missing value for --endpoint";
                return false;
            }
            args.endpoint = argv[++index];
            continue;
        }
        if (token == "--max-requests") {
            if ((index + 1) >= argc) {
                error_message = "missing value for --max-requests";
                return false;
            }
            const std::string raw_value = argv[++index];
            if (!ParsePositiveInt(raw_value, args.max_requests)) {
                error_message = "invalid --max-requests value: " + raw_value;
                return false;
            }
            continue;
        }

        error_message = "unsupported arg: " + token;
        return false;
    }

    args_out = args;
    return true;
}

nlohmann::json ParseRequestOrFallback(const std::string& request_payload) {
    try {
        const nlohmann::json parsed = nlohmann::json::parse(request_payload);
        if (parsed.is_object()) {
            return parsed;
        }

        nlohmann::json wrapper = nlohmann::json::object();
        wrapper["raw"] = parsed.dump();
        return wrapper;
    } catch (...) {
        nlohmann::json wrapper = nlohmann::json::object();
        wrapper["raw"] = request_payload;
        return wrapper;
    }
}

}  // namespace

int main(int argc, char** argv) {
    ServerArgs args;
    std::string parse_error;
    if (!ParseArgs(argc, argv, args, parse_error)) {
        std::cerr << parse_error << std::endl;
        return 2;
    }

    std::string factory_error;
    std::unique_ptr<ProcessInterface::Ipc::IIpcServer> server =
        ProcessInterface::Ipc::CreateIpcServer(args.backend, factory_error);
    if (!server) {
        std::cerr << factory_error << std::endl;
        return 2;
    }

    std::string bind_error;
    if (!server->Bind(args.endpoint, bind_error)) {
        std::cerr << bind_error << std::endl;
        return 2;
    }

    std::cout << "SERVER listening on " << args.endpoint
              << " (max requests: " << args.max_requests << ")" << std::endl;

    ProcessInterface::Ipc::IIpcServer* server_ptr = server.get();
    int request_count = 0;

    server->SetRequestHandler([&](const std::string& request_payload) -> std::string {
        request_count += 1;

        const nlohmann::json request_json = ParseRequestOrFallback(request_payload);
        const std::string message = request_json.value("message", std::string("<empty>"));

        nlohmann::json response_json = nlohmann::json::object();
        response_json["ok"] = true;
        response_json["sequence"] = request_count;
        response_json["reply"] = std::string("app-b heard: ") + message;
        response_json["received"] = request_json;

        nlohmann::json event_json = nlohmann::json::object();
        event_json["sequence"] = request_count;
        event_json["request"] = request_json;
        event_json["reply"] = response_json["reply"];
        std::cout << "EVENT " << event_json.dump() << std::endl;

        if (request_count >= args.max_requests) {
            server_ptr->Stop();
        }

        return response_json.dump();
    });

    std::string run_error;
    if (!server->Run(run_error)) {
        std::cerr << run_error << std::endl;
        return 2;
    }

    std::cout << "SERVER stopped after " << request_count << " request(s)" << std::endl;
    return 0;
}
