#include "ZmqIpcClient.h"

#include <cerrno>
#include <cstring>
#include <string>

#include <zmq.h>

namespace ProcessInterface {
namespace Ipc {

ZmqIpcClient::ZmqIpcClient()
    : socket_(NULL) {}

ZmqIpcClient::~ZmqIpcClient() {
    if (socket_ != NULL) {
        zmq_close(socket_);
        socket_ = NULL;
    }
}

bool ZmqIpcClient::Connect(const std::string& endpoint, std::string& error_message) {
    if (!context_.valid()) {
        error_message = "failed to initialize zmq context";
        return false;
    }

    if (socket_ != NULL) {
        zmq_close(socket_);
        socket_ = NULL;
    }

    socket_ = zmq_socket(context_.raw(), ZMQ_REQ);
    if (socket_ == NULL) {
        error_message = std::string("zmq_socket failed: ") + std::strerror(errno);
        return false;
    }

    const int linger = 0;
    zmq_setsockopt(socket_, ZMQ_LINGER, &linger, sizeof(linger));

    if (zmq_connect(socket_, endpoint.c_str()) != 0) {
        error_message = std::string("zmq_connect failed: ") + std::strerror(errno);
        zmq_close(socket_);
        socket_ = NULL;
        return false;
    }

    endpoint_ = endpoint;
    return true;
}

bool ZmqIpcClient::Request(
    const std::string& request_payload,
    std::string& response_payload,
    std::string& error_message) {
    if (socket_ == NULL) {
        error_message = "ipc client not connected";
        return false;
    }

    if (zmq_send(socket_, request_payload.data(), request_payload.size(), 0) < 0) {
        error_message = std::string("zmq_send failed: ") + std::strerror(errno);
        return false;
    }

    zmq_msg_t message;
    zmq_msg_init(&message);
    const int rc = zmq_msg_recv(&message, socket_, 0);
    if (rc < 0) {
        error_message = std::string("zmq_recv failed: ") + std::strerror(errno);
        zmq_msg_close(&message);
        return false;
    }

    const char* data = static_cast<const char*>(zmq_msg_data(&message));
    const std::size_t size = static_cast<std::size_t>(zmq_msg_size(&message));
    response_payload.assign(data, data + size);
    zmq_msg_close(&message);
    return true;
}

}  // namespace Ipc
}  // namespace ProcessInterface
