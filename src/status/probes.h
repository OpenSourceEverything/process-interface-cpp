#ifndef PROCESS_INTERFACE_STATUS_PROBES_H
#define PROCESS_INTERFACE_STATUS_PROBES_H

#include <string>
#include <vector>

namespace ProcessInterface {
namespace Status {

struct ProcessProbeResult {
    bool running;
    int pid;
    std::vector<int> pids;
};

class IStatusProbes {
public:
    virtual ~IStatusProbes() {}

    virtual ProcessProbeResult QueryProcessByName(const std::string& process_name) const = 0;
    virtual bool CheckPortListening(const std::string& host, int port, int timeout_ms) const = 0;
};

class PlatformStatusProbes : public IStatusProbes {
public:
    virtual ProcessProbeResult QueryProcessByName(const std::string& process_name) const;
    virtual bool CheckPortListening(const std::string& host, int port, int timeout_ms) const;
};

}  // namespace Status
}  // namespace ProcessInterface

#endif  // PROCESS_INTERFACE_STATUS_PROBES_H
