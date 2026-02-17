#ifndef PROCESS_INTERFACE_STATUS_CONTEXT_H
#define PROCESS_INTERFACE_STATUS_CONTEXT_H

#include "fs.h"
#include <string>
#include <vector>

namespace ProcessInterface {
namespace Status {

struct ProcessProbeResult {
    bool running;
    int pid;
    std::vector<int> pids;
};

struct StatusContext {
    std::string app_id;
    fs::path repo_root;
};

ProcessProbeResult QueryProcessByName(const std::string& process_name);
bool CheckPortListening(const std::string& host, int port, int timeout_ms);

}  // namespace Status
}  // namespace ProcessInterface

#endif  // PROCESS_INTERFACE_STATUS_CONTEXT_H

