#include "error_map.h"

namespace ProcessInterface {
namespace Status {

std::string ToIpcErrorCode(StatusErrorCode code) {
    switch (code) {
        case StatusErrorCode::kUnsupportedApp:
            return "E_UNSUPPORTED_APP";
        case StatusErrorCode::kSpecMissing:
        case StatusErrorCode::kCollectFailed:
        case StatusErrorCode::kSnapshotWriteFailed:
            return "E_NATIVE_UNAVAILABLE";
        case StatusErrorCode::kSpecInvalid:
        case StatusErrorCode::kNone:
        default:
            return "E_INTERNAL";
    }
}

}  // namespace Status
}  // namespace ProcessInterface
