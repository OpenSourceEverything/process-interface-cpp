#include "IpcFactory.h"

#include "../impl/ZmqIpcClient.h"
#include "../impl/ZmqIpcServer.h"

namespace ProcessInterface {
namespace Ipc {

std::unique_ptr<IIpcServer> CreateIpcServer(
    const std::string& backend,
    std::string& error_message) {
    if (backend == "zmq") {
        return std::unique_ptr<IIpcServer>(new ZmqIpcServer());
    }

    error_message = "unsupported ipc backend: " + backend;
    return std::unique_ptr<IIpcServer>();
}

std::unique_ptr<IIpcClient> CreateIpcClient(
    const std::string& backend,
    std::string& error_message) {
    if (backend == "zmq") {
        return std::unique_ptr<IIpcClient>(new ZmqIpcClient());
    }

    error_message = "unsupported ipc backend: " + backend;
    return std::unique_ptr<IIpcClient>();
}

}  // namespace Ipc
}  // namespace ProcessInterface
