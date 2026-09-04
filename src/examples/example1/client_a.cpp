#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "../../ipc/factory/IpcFactory.h"
#include "../../../external/nlohmann/json.hpp"

namespace {

struct ClientAArgs {
    std::string backend;
    std::string endpoint;
    int count;
    int send_delay_ms;
    int poll_delay_ms;
    int max_empty_polls;
};

bool ParsePositiveInt(const std::string& value, int& out_value) {
    char* end = NULL;
    const long parsed = std::strtol(value.c_str(), &end, 10);
    if (end == value.c_str() || *end != '\0') {
        return false;
    }
    if (parsed <= 0 || parsed > 1000000) {
        return false;
    }
    out_value = static_cast<int>(parsed);
    return true;
}

bool ParseArgs(int argc, char** argv, ClientAArgs& args_out, std::string& error_message) {
    ClientAArgs args;
    args.backend = "zmq";
    args.endpoint = "tcp://127.0.0.1:9000";
    args.count = 3;
    args.send_delay_ms = 150;
    args.poll_delay_ms = 100;
    args.max_empty_polls = 600;

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
        if (token == "--send-delay-ms") {
            if ((index + 1) >= argc) {
                error_message = "missing value for --send-delay-ms";
                return false;
            }
            const std::string raw_value = argv[++index];
            if (!ParsePositiveInt(raw_value, args.send_delay_ms)) {
                error_message = "invalid --send-delay-ms value: " + raw_value;
                return false;
            }
            continue;
        }
        if (token == "--poll-delay-ms") {
            if ((index + 1) >= argc) {
                error_message = "missing value for --poll-delay-ms";
                return false;
            }
            const std::string raw_value = argv[++index];
            if (!ParsePositiveInt(raw_value, args.poll_delay_ms)) {
                error_message = "invalid --poll-delay-ms value: " + raw_value;
                return false;
            }
            continue;
        }
        if (token == "--max-empty-polls") {
            if ((index + 1) >= argc) {
                error_message = "missing value for --max-empty-polls";
                return false;
            }
            const std::string raw_value = argv[++index];
            if (!ParsePositiveInt(raw_value, args.max_empty_polls)) {
                error_message = "invalid --max-empty-polls value: " + raw_value;
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

bool SendJsonRequest(
    ProcessInterface::Ipc::IIpcClient& client,
    const nlohmann::json& request_json,
    nlohmann::json& response_json,
    std::string& error_message) {
    std::string response_payload;
    std::string request_error;
    if (!client.Request(request_json.dump(), response_payload, request_error)) {
        error_message = request_error;
        return false;
    }

    try {
        response_json = nlohmann::json::parse(response_payload);
    } catch (const std::exception& ex) {
        error_message = std::string("invalid JSON response: ") + ex.what();
        return false;
    }

    if (!response_json.is_object()) {
        error_message = "response must be a JSON object";
        return false;
    }

    if (!response_json.value("ok", false)) {
        error_message = response_json.value("error", std::string("host returned error"));
        return false;
    }

    return true;
}

}  // namespace

int main(int argc, char** argv) {
    ClientAArgs args;
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

    std::cout << "CLIENT_A connected to host at " << args.endpoint << std::endl;
    std::cout << "CLIENT_A sending " << args.count << " message(s) to client-b through host"
              << std::endl;

    int sequence = 0;
    for (sequence = 1; sequence <= args.count; ++sequence) {
        nlohmann::json request_json = nlohmann::json::object();
        request_json["action"] = "send";
        request_json["from"] = "client-a";
        request_json["to"] = "client-b";
        request_json["sequence"] = sequence;
        request_json["message"] = std::string("hello from client-a #") + std::to_string(sequence);

        nlohmann::json response_json;
        std::string request_error;
        if (!SendJsonRequest(*client, request_json, response_json, request_error)) {
            std::cerr << request_error << std::endl;
            return 2;
        }

        std::cout << "CLIENT_A sent #" << sequence << " -> " << response_json.dump() << std::endl;
        if (sequence < args.count) {
            std::this_thread::sleep_for(std::chrono::milliseconds(args.send_delay_ms));
        }
    }

    std::cout << "CLIENT_A waiting for " << args.count
              << " reply message(s) from client-b via host" << std::endl;

    int replies_received = 0;
    int empty_polls = 0;
    while (replies_received < args.count) {
        nlohmann::json poll_request = nlohmann::json::object();
        poll_request["action"] = "poll";
        poll_request["client"] = "client-a";

        nlohmann::json poll_response;
        std::string poll_error;
        if (!SendJsonRequest(*client, poll_request, poll_response, poll_error)) {
            std::cerr << poll_error << std::endl;
            return 2;
        }

        if (!poll_response.value("has_message", false)) {
            empty_polls += 1;
            if (empty_polls > args.max_empty_polls) {
                std::cerr << "CLIENT_A timed out waiting for replies" << std::endl;
                return 3;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(args.poll_delay_ms));
            continue;
        }

        empty_polls = 0;
        replies_received += 1;
        std::cout << "CLIENT_A received reply " << poll_response["message"].dump() << std::endl;
    }

    nlohmann::json shutdown_request = nlohmann::json::object();
    shutdown_request["action"] = "shutdown";
    shutdown_request["client"] = "client-a";

    nlohmann::json shutdown_response;
    std::string shutdown_error;
    if (!SendJsonRequest(*client, shutdown_request, shutdown_response, shutdown_error)) {
        std::cerr << shutdown_error << std::endl;
        return 2;
    }

    std::cout << "CLIENT_A requested host shutdown -> " << shutdown_response.dump() << std::endl;
    return 0;
}
