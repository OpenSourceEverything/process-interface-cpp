#ifndef PROCESS_INTERFACE_PLATFORM_PROCESS_PROBE_H
#define PROCESS_INTERFACE_PLATFORM_PROCESS_PROBE_H

#include <string>
#include <vector>

namespace ProcessInterface {
namespace Platform {

struct ProcessQueryResult {
    bool running;
    int pid;
    std::vector<int> pids;
    std::string error_message;
};

ProcessQueryResult QueryProcessByName(const std::string& process_name);

}  // namespace Platform
}  // namespace ProcessInterface

#endif  // PROCESS_INTERFACE_PLATFORM_PROCESS_PROBE_H
