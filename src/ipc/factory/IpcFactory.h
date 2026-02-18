#ifndef PROCESS_INTERFACE_IPC_FACTORY_IPC_FACTORY_H
#define PROCESS_INTERFACE_IPC_FACTORY_IPC_FACTORY_H

#include <memory>
#include <string>

#include "../IpcClient.h"
#include "../IpcServer.h"

namespace ProcessInterface {
namespace Ipc {

std::unique_ptr<IIpcServer> CreateIpcServer(
    const std::string& backend,
    std::string& error_message);

std::unique_ptr<IIpcClient> CreateIpcClient(
    const std::string& backend,
    std::string& error_message);

}  // namespace Ipc
}  // namespace ProcessInterface

#endif  // PROCESS_INTERFACE_IPC_FACTORY_IPC_FACTORY_H
