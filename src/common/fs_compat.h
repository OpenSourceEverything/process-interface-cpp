#ifndef PROCESS_INTERFACE_COMMON_FS_COMPAT_H
#define PROCESS_INTERFACE_COMMON_FS_COMPAT_H

#if defined(__has_include)
#if __has_include(<filesystem>)
#include <filesystem>
namespace ProcessInterface {
namespace Common {
namespace fs = std::filesystem;
}  // namespace Common
}  // namespace ProcessInterface
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
namespace ProcessInterface {
namespace Common {
namespace fs = std::experimental::filesystem;
}  // namespace Common
}  // namespace ProcessInterface
#else
#error "No filesystem implementation available for ProcessInterface::Common"
#endif
#else
#include <filesystem>
namespace ProcessInterface {
namespace Common {
namespace fs = std::filesystem;
}  // namespace Common
}  // namespace ProcessInterface
#endif

#endif  // PROCESS_INTERFACE_COMMON_FS_COMPAT_H
