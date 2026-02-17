#ifndef PROCESS_INTERFACE_STATUS_DEBUG_H
#define PROCESS_INTERFACE_STATUS_DEBUG_H

#include <string>

namespace ProcessInterface {
namespace Status {

bool DebugEnabled();
void DebugLog(const std::string& message);

}  // namespace Status
}  // namespace ProcessInterface

#endif  // PROCESS_INTERFACE_STATUS_DEBUG_H
