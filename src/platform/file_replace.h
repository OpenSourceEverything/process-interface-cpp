#ifndef PROCESS_INTERFACE_PLATFORM_FILE_REPLACE_H
#define PROCESS_INTERFACE_PLATFORM_FILE_REPLACE_H

#include <string>

#include "../common/fs_compat.h"

namespace ProcessInterface {
namespace Platform {

bool AtomicReplaceFile(const Common::fs::path& target_path, const std::string& contents, std::string& error_message);

}  // namespace Platform
}  // namespace ProcessInterface

#endif  // PROCESS_INTERFACE_PLATFORM_FILE_REPLACE_H
