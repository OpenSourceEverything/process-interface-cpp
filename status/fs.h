#ifndef PROCESS_INTERFACE_STATUS_FS_H
#define PROCESS_INTERFACE_STATUS_FS_H

#if defined(__cpp_lib_filesystem) && (__cpp_lib_filesystem >= 201703L)
#include "fs.h"
namespace ProcessInterface {
namespace Status {
namespace fs = std::filesystem;
}  // namespace Status
}  // namespace ProcessInterface
#elif defined(__cpp_lib_experimental_filesystem) || defined(_MSC_VER)
#include <experimental/filesystem>
namespace ProcessInterface {
namespace Status {
namespace fs = std::experimental::filesystem;
}  // namespace Status
}  // namespace ProcessInterface
#else
#error "No filesystem implementation available for ProcessInterface::Status"
#endif

#endif  // PROCESS_INTERFACE_STATUS_FS_H

