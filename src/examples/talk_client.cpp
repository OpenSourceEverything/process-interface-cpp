#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "../ipc/factory/IpcFactory.h"
#include "../../external/nlohmann/json.hpp"

namespace {

struct ClientArgs {
    std::string backend;
    std::string endpoint;
    std::string from;
    std::string to;
    int count;
    int delay_ms;
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

bool ParseArgs(int argc, char** argv, ClientArgs& args_out, std::string& error_message) {
    ClientArgs args;
    args.backend = "zmq";
    args.endpoint = "tcp://127.0.0.1:5580";
    args.from = "app-a";
    args.to = "app-b";
    args.count = 4;
    args.delay_ms = 150;

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
        if (token == "--from") {
            if ((index + 1) >= argc) {
                error_message = "missing value for --from";
                return false;
            }
            args.from = argv[++index];
            continue;
        }
        if (token == "--to") {
            if ((index + 1) >= argc) {
                error_message = "missing value for --to";
                return false;
            }
            args.to = argv[++index];
            continue;
        }
        if (token == "--count") {
            if ((index + 1) >= argc) {
                error_message = "missing value for --count";
                return false;
            }
            const std::string raw_value = argv[++index];
            if (!ParsePositiveInt(raw_value, args.count)) {
                error_message = "invalid --count value: " + raw_value;
                return false;
            }
            continue;
        }
        if (token == "--delay-ms") {
            if ((index + 1) >= argc) {
                error_message = "missing value for --delay-ms";
                return false;
            }
            const std::string raw_value = argv[++index];
            if (!ParsePositiveInt(raw_value, args.delay_ms)) {
                error_message = "invalid --delay-ms value: " + raw_value;
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

    int sequence = 0;
    for (sequence = 1; sequence <= args.count; ++sequence) {
        nlohmann::json request_json = nlohmann::json::object();
        request_json["from"] = args.from;
        request_json["to"] = args.to;
        request_json["sequence"] = sequence;
        request_json["message"] = std::string("hello #") + std::to_string(sequence);

        const std::string request_payload = request_json.dump();
        std::cout << "CLIENT request " << request_payload << std::endl;

        std::string response_payload;
        std::string request_error;
        if (!client->Request(request_payload, response_payload, request_error)) {
            std::cerr << request_error << std::endl;
            return 2;
        }

        std::cout << "CLIENT response " << response_payload << std::endl;

        if (sequence < args.count) {
            std::this_thread::sleep_for(std::chrono::milliseconds(args.delay_ms));
        }
    }

    return 0;
}
