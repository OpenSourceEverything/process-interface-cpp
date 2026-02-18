#ifndef PROCESS_INTERFACE_STATUS_CONTEXT_H
#define PROCESS_INTERFACE_STATUS_CONTEXT_H

#include <string>

#include "fs.h"
#include "probes.h"

namespace ProcessInterface {
namespace Status {

struct StatusContext {
    std::string app_id;
    fs::path repo_root;
    const IStatusProbes* probes;
};

}  // namespace Status
}  // namespace ProcessInterface

#endif  // PROCESS_INTERFACE_STATUS_CONTEXT_H
