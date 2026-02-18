#include "time_utils.h"

#include <chrono>
#include <ctime>

namespace ProcessInterface {
namespace Common {

std::string CurrentUtcIso8601() {
    const std::time_t now = std::time(NULL);
    std::tm utc_tm = {};
    const std::tm* utc_ptr = std::gmtime(&now);
    if (utc_ptr != NULL) {
        utc_tm = *utc_ptr;
    }

    char buffer[64];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc_tm);
    return std::string(buffer);
}

long long CurrentEpochMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

}  // namespace Common
}  // namespace ProcessInterface
