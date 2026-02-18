#include "ZmqIpcServer.h"

#include <cerrno>
#include <cstring>
#include <string>

#include <zmq.h>

namespace ProcessInterface {
namespace Ipc {

ZmqIpcServer::ZmqIpcServer()
    : socket_(NULL),
      stop_requested_(false) {}

ZmqIpcServer::~ZmqIpcServer() {
    if (socket_ != NULL) {
        zmq_close(socket_);
        socket_ = NULL;
    }
}

bool ZmqIpcServer::Bind(const std::string& endpoint, std::string& error_message) {
    if (!context_.valid()) {
        error_message = "failed to initialize zmq context";
        return false;
    }

    if (socket_ != NULL) {
        zmq_close(socket_);
        socket_ = NULL;
    }

    socket_ = zmq_socket(context_.raw(), ZMQ_REP);
    if (socket_ == NULL) {
        error_message = std::string("zmq_socket failed: ") + std::strerror(errno);
        return false;
    }

    const int linger = 0;
    zmq_setsockopt(socket_, ZMQ_LINGER, &linger, sizeof(linger));

    if (zmq_bind(socket_, endpoint.c_str()) != 0) {
        error_message = std::string("zmq_bind failed: ") + std::strerror(errno);
        zmq_close(socket_);
        socket_ = NULL;
        return false;
    }

    return true;
}

void ZmqIpcServer::SetRequestHandler(const RequestHandler& handler) {
    handler_ = handler;
}

bool ZmqIpcServer::Run(std::string& error_message) {
    if (socket_ == NULL) {
        error_message = "ipc server is not bound";
        return false;
    }
    if (!handler_) {
        error_message = "ipc request handler is not set";
        return false;
    }

    stop_requested_ = false;

    while (!stop_requested_) {
        zmq_msg_t message;
        zmq_msg_init(&message);
        const int received = zmq_msg_recv(&message, socket_, 0);
        if (received < 0) {
            const int err = errno;
            zmq_msg_close(&message);
            if (err == EINTR) {
                continue;
            }
            error_message = std::string("zmq_recv failed: ") + std::strerror(err);
            return false;
        }

        const char* data = static_cast<const char*>(zmq_msg_data(&message));
        const std::size_t size = static_cast<std::size_t>(zmq_msg_size(&message));
        const std::string request_payload(data, data + size);
        zmq_msg_close(&message);

        const std::string response_payload = handler_(request_payload);
        if (zmq_send(socket_, response_payload.data(), response_payload.size(), 0) < 0) {
            error_message = std::string("zmq_send failed: ") + std::strerror(errno);
            return false;
        }
    }

    return true;
}

void ZmqIpcServer::Stop() {
    stop_requested_ = true;
}

}  // namespace Ipc
}  // namespace ProcessInterface
