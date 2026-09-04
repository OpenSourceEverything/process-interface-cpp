#include <deque>
#include <iostream>
#include <map>
#include <memory>
#include <string>

#include "../../ipc/factory/IpcFactory.h"
#include "../../../external/nlohmann/json.hpp"

namespace {

struct HostArgs {
    std::string backend;
    std::string endpoint;
};

bool ParseArgs(int argc, char** argv, HostArgs& args_out, std::string& error_message) {
    HostArgs args;
    args.backend = "zmq";
    args.endpoint = "tcp://127.0.0.1:9000";

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

        error_message = "unsupported arg: " + token;
        return false;
    }

    args_out = args;
    return true;
}

bool ParseJsonObject(const std::string& payload, nlohmann::json& out_json, std::string& error_message) {
    try {
        out_json = nlohmann::json::parse(payload);
    } catch (const std::exception& ex) {
        error_message = std::string("invalid JSON request: ") + ex.what();
        return false;
    }

    if (!out_json.is_object()) {
        error_message = "request must be a JSON object";
        return false;
    }

    return true;
}

bool ReadRequiredString(
    const nlohmann::json& object_json,
    const char* key,
    std::string& value_out,
    std::string& error_message) {
    if (!object_json.contains(key)) {
        error_message = std::string("missing required field: ") + key;
        return false;
    }
    const nlohmann::json& value_json = object_json[key];
    if (!value_json.is_string()) {
        error_message = std::string("field must be string: ") + key;
        return false;
    }
    value_out = value_json.get<std::string>();
    return true;
}

std::string ErrorResponse(const std::string& error_message) {
    nlohmann::json response_json = nlohmann::json::object();
    response_json["ok"] = false;
    response_json["error"] = error_message;
    return response_json.dump();
}

}  // namespace

int main(int argc, char** argv) {
    HostArgs args;
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

    std::map<std::string, std::deque<nlohmann::json> > inbox_by_client;
    int total_requests = 0;
    int total_routed_messages = 0;
    int total_delivered_messages = 0;
    ProcessInterface::Ipc::IIpcServer* server_ptr = server.get();

    server->SetRequestHandler([&](
                                  const std::string& request_payload) -> std::string {
        total_requests += 1;

        nlohmann::json request_json;
        std::string request_error;
        if (!ParseJsonObject(request_payload, request_json, request_error)) {
            return ErrorResponse(request_error);
        }

        std::string action;
        if (!ReadRequiredString(request_json, "action", action, request_error)) {
            return ErrorResponse(request_error);
        }

        if (action == "send") {
            std::string from_client;
            std::string to_client;
            std::string message_text;
            if (!ReadRequiredString(request_json, "from", from_client, request_error)) {
                return ErrorResponse(request_error);
            }
            if (!ReadRequiredString(request_json, "to", to_client, request_error)) {
                return ErrorResponse(request_error);
            }
            if (!ReadRequiredString(request_json, "message", message_text, request_error)) {
                return ErrorResponse(request_error);
            }

            nlohmann::json envelope_json = nlohmann::json::object();
            envelope_json["from"] = from_client;
            envelope_json["to"] = to_client;
            envelope_json["message"] = message_text;
            if (request_json.contains("sequence")) {
                envelope_json["sequence"] = request_json["sequence"];
            }

            if (request_json.contains("meta")) {
                envelope_json["meta"] = request_json["meta"];
            }

            std::deque<nlohmann::json>& inbox = inbox_by_client[to_client];
            inbox.push_back(envelope_json);
            total_routed_messages += 1;

            std::cout << "HOST ROUTE " << from_client << " -> " << to_client
                      << " message=\"" << message_text << "\""
                      << " queue_depth=" << inbox.size() << std::endl;

            nlohmann::json response_json = nlohmann::json::object();
            response_json["ok"] = true;
            response_json["action"] = "send";
            response_json["accepted"] = true;
            response_json["queued_for"] = to_client;
            response_json["queue_depth"] = static_cast<int>(inbox.size());
            return response_json.dump();
        }

        if (action == "poll") {
            std::string client_name;
            if (!ReadRequiredString(request_json, "client", client_name, request_error)) {
                return ErrorResponse(request_error);
            }

            std::deque<nlohmann::json>& inbox = inbox_by_client[client_name];

            nlohmann::json response_json = nlohmann::json::object();
            response_json["ok"] = true;
            response_json["action"] = "poll";
            if (inbox.empty()) {
                response_json["has_message"] = false;
                return response_json.dump();
            }

            const nlohmann::json message_json = inbox.front();
            inbox.pop_front();
            total_delivered_messages += 1;

            std::cout << "HOST DELIVER to " << client_name
                      << " message=" << message_json.dump() << std::endl;

            response_json["has_message"] = true;
            response_json["message"] = message_json;
            return response_json.dump();
        }

        if (action == "shutdown") {
            std::string requested_by = "<unknown>";
            if (request_json.contains("client") && request_json["client"].is_string()) {
                requested_by = request_json["client"].get<std::string>();
            }

            std::cout << "HOST STOP requested by " << requested_by << std::endl;

            nlohmann::json response_json = nlohmann::json::object();
            response_json["ok"] = true;
            response_json["action"] = "shutdown";
            response_json["stopping"] = true;

            server_ptr->Stop();
            return response_json.dump();
        }

        return ErrorResponse("unsupported action: " + action);
    });

    std::cout << "HOST listening on " << args.endpoint << std::endl;
    std::cout << "HOST protocol actions: send, poll, shutdown" << std::endl;

    std::string run_error;
    if (!server->Run(run_error)) {
        std::cerr << run_error << std::endl;
        return 2;
    }

    std::cout << "HOST stopped (requests=" << total_requests
              << ", routed=" << total_routed_messages
              << ", delivered=" << total_delivered_messages << ")" << std::endl;
    return 0;
}
