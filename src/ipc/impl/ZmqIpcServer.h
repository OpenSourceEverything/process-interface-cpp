#ifndef PROCESS_INTERFACE_IPC_IMPL_ZMQ_IPC_SERVER_H
#define PROCESS_INTERFACE_IPC_IMPL_ZMQ_IPC_SERVER_H

#include <string>

#include "../IpcServer.h"
#include "ZmqContext.h"

namespace ProcessInterface {
namespace Ipc {

class ZmqIpcServer : public IIpcServer {
public:
    ZmqIpcServer();
    virtual ~ZmqIpcServer();

    virtual bool Bind(const std::string& endpoint, std::string& error_message);
    virtual void SetRequestHandler(const RequestHandler& handler);
    virtual bool Run(std::string& error_message);
    virtual void Stop();

private:
    ZmqContext context_;
    void* socket_;
    RequestHandler handler_;
    bool stop_requested_;
};

}  // namespace Ipc
}  // namespace ProcessInterface

#endif  // PROCESS_INTERFACE_IPC_IMPL_ZMQ_IPC_SERVER_H
