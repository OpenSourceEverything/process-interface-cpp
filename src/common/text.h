#ifndef PROCESS_INTERFACE_COMMON_TEXT_H
#define PROCESS_INTERFACE_COMMON_TEXT_H

#include <cstddef>
#include <string>
#include <vector>

namespace ProcessInterface {
namespace Common {

std::string TrimCopy(const std::string& value);
std::vector<std::string> Split(const std::string& value, char delimiter);
std::string Join(const std::vector<std::string>& parts, std::size_t start_index, const std::string& delimiter);
std::vector<std::string> SplitNonEmptyLines(const std::string& value);

}  // namespace Common
}  // namespace ProcessInterface

#endif  // PROCESS_INTERFACE_COMMON_TEXT_H
