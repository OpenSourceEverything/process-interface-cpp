#ifndef PROCESS_INTERFACE_COMMON_TIME_UTILS_H
#define PROCESS_INTERFACE_COMMON_TIME_UTILS_H

#include <string>

namespace ProcessInterface {
namespace Common {

std::string CurrentUtcIso8601();
long long CurrentEpochMs();

}  // namespace Common
}  // namespace ProcessInterface

#endif  // PROCESS_INTERFACE_COMMON_TIME_UTILS_H
