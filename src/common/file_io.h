#ifndef PROCESS_INTERFACE_COMMON_FILE_IO_H
#define PROCESS_INTERFACE_COMMON_FILE_IO_H

#include <string>

#include "fs_compat.h"

namespace ProcessInterface {
namespace Common {

bool ReadTextFile(const fs::path& path, std::string& text_out);
bool WriteTextFile(const fs::path& path, const std::string& text, std::string& error_message);

}  // namespace Common
}  // namespace ProcessInterface

#endif  // PROCESS_INTERFACE_COMMON_FILE_IO_H
