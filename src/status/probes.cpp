#include "probes.h"

#include "../platform/port_probe.h"
#include "../platform/process_probe.h"

namespace ProcessInterface {
namespace Status {

ProcessProbeResult PlatformStatusProbes::QueryProcessByName(const std::string& process_name) const {
    const ProcessInterface::Platform::ProcessQueryResult query_result =
        ProcessInterface::Platform::QueryProcessByName(process_name);

    ProcessProbeResult result;
    result.running = query_result.running;
    result.pid = query_result.pid;
    result.pids = query_result.pids;
    return result;
}

bool PlatformStatusProbes::CheckPortListening(const std::string& host, int port, int timeout_ms) const {
    return ProcessInterface::Platform::CheckPortListening(host, port, timeout_ms);
}

}  // namespace Status
}  // namespace ProcessInterface
