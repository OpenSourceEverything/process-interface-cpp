#ifndef PROCESS_INTERFACE_IPC_TYPES_H
#define PROCESS_INTERFACE_IPC_TYPES_H

#include <functional>
#include <string>

namespace ProcessInterface {
namespace Ipc {

typedef std::function<std::string(const std::string&)> RequestHandler;

}  // namespace Ipc
}  // namespace ProcessInterface

#endif  // PROCESS_INTERFACE_IPC_TYPES_H
