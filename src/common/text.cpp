#include "text.h"

#include <cctype>
#include <sstream>

namespace ProcessInterface {
namespace Common {

std::string TrimCopy(const std::string& value) {
    if (value.empty()) {
        return std::string();
    }

    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
        ++start;
    }
    if (start >= value.size()) {
        return std::string();
    }

    std::size_t end = value.size() - 1;
    while (end > start && std::isspace(static_cast<unsigned char>(value[end])) != 0) {
        --end;
    }
    return value.substr(start, end - start + 1);
}

std::vector<std::string> Split(const std::string& value, char delimiter) {
    std::vector<std::string> parts;
    std::string current;

    std::size_t index;
    for (index = 0; index < value.size(); ++index) {
        const char c = value[index];
        if (c == delimiter) {
            parts.push_back(current);
            current.clear();
            continue;
        }
        current.push_back(c);
    }
    parts.push_back(current);
    return parts;
}

std::string Join(const std::vector<std::string>& parts, std::size_t start_index, const std::string& delimiter) {
    if (start_index >= parts.size()) {
        return std::string();
    }

    std::ostringstream stream;
    std::size_t index;
    for (index = start_index; index < parts.size(); ++index) {
        if (index > start_index) {
            stream << delimiter;
        }
        stream << parts[index];
    }
    return stream.str();
}

std::vector<std::string> SplitNonEmptyLines(const std::string& value) {
    std::vector<std::string> lines;
    std::istringstream stream(value);

    std::string line;
    while (std::getline(stream, line)) {
        const std::string trimmed = TrimCopy(line);
        if (!trimmed.empty()) {
            lines.push_back(trimmed);
        }
    }
    return lines;
}

}  // namespace Common
}  // namespace ProcessInterface
