#ifndef PROCESS_INTERFACE_IPC_CLIENT_H
#define PROCESS_INTERFACE_IPC_CLIENT_H

#include <string>

namespace ProcessInterface {
namespace Ipc {

class IIpcClient {
public:
    virtual ~IIpcClient() {}

    virtual bool Connect(const std::string& endpoint, std::string& error_message) = 0;
    virtual bool Request(
        const std::string& request_payload,
        std::string& response_payload,
        std::string& error_message) = 0;
};

}  // namespace Ipc
}  // namespace ProcessInterface

#endif  // PROCESS_INTERFACE_IPC_CLIENT_H
