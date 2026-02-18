#ifndef PROCESS_INTERFACE_IPC_SERVER_H
#define PROCESS_INTERFACE_IPC_SERVER_H

#include <string>

#include "IpcTypes.h"

namespace ProcessInterface {
namespace Ipc {

class IIpcServer {
public:
    virtual ~IIpcServer() {}

    virtual bool Bind(const std::string& endpoint, std::string& error_message) = 0;
    virtual void SetRequestHandler(const RequestHandler& handler) = 0;
    virtual bool Run(std::string& error_message) = 0;
    virtual void Stop() = 0;
};

}  // namespace Ipc
}  // namespace ProcessInterface

#endif  // PROCESS_INTERFACE_IPC_SERVER_H
