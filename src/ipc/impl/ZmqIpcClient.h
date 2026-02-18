#ifndef PROCESS_INTERFACE_IPC_IMPL_ZMQ_IPC_CLIENT_H
#define PROCESS_INTERFACE_IPC_IMPL_ZMQ_IPC_CLIENT_H

#include <string>

#include "../IpcClient.h"
#include "ZmqContext.h"

namespace ProcessInterface {
namespace Ipc {

class ZmqIpcClient : public IIpcClient {
public:
    ZmqIpcClient();
    virtual ~ZmqIpcClient();

    virtual bool Connect(const std::string& endpoint, std::string& error_message);
    virtual bool Request(
        const std::string& request_payload,
        std::string& response_payload,
        std::string& error_message);

private:
    ZmqContext context_;
    void* socket_;
    std::string endpoint_;
};

}  // namespace Ipc
}  // namespace ProcessInterface

#endif  // PROCESS_INTERFACE_IPC_IMPL_ZMQ_IPC_CLIENT_H
