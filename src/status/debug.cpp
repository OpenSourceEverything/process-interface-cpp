#include "debug.h"

#include <cstdlib>
#include <iostream>

namespace ProcessInterface {
namespace Status {

bool DebugEnabled() {
    const char* raw = std::getenv("PROCESS_INTERFACE_DEBUG_STATUS");
    if (raw == NULL) {
        return false;
    }
    const std::string value(raw);
    return value == "1" || value == "true" || value == "TRUE" || value == "yes" || value == "YES";
}

void DebugLog(const std::string& message) {
    if (!DebugEnabled()) {
        return;
    }
    std::cerr << "[ProcessInterface::Status] " << message << std::endl;
}

}  // namespace Status
}  // namespace ProcessInterface
