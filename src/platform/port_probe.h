#ifndef PROCESS_INTERFACE_PLATFORM_PORT_PROBE_H
#define PROCESS_INTERFACE_PLATFORM_PORT_PROBE_H

#include <string>

namespace ProcessInterface {
namespace Platform {

bool CheckPortListening(const std::string& host, int port, int timeout_ms);

}  // namespace Platform
}  // namespace ProcessInterface

#endif  // PROCESS_INTERFACE_PLATFORM_PORT_PROBE_H
