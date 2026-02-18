#ifndef PROCESS_INTERFACE_STATUS_ERROR_MAP_H
#define PROCESS_INTERFACE_STATUS_ERROR_MAP_H

#include <string>

namespace ProcessInterface {
namespace Status {

enum class StatusErrorCode {
    kNone = 0,
    kUnsupportedApp,
    kSpecMissing,
    kSpecInvalid,
    kCollectFailed,
    kSnapshotWriteFailed,
};

std::string ToIpcErrorCode(StatusErrorCode code);

}  // namespace Status
}  // namespace ProcessInterface

#endif  // PROCESS_INTERFACE_STATUS_ERROR_MAP_H
