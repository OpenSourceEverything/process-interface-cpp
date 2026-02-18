#include "file_io.h"

#include <fstream>
#include <sstream>

namespace ProcessInterface {
namespace Common {

bool ReadTextFile(const fs::path& path, std::string& text_out) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return false;
    }

    std::ostringstream stream;
    stream << input.rdbuf();
    text_out = stream.str();
    return true;
}

bool WriteTextFile(const fs::path& path, const std::string& text, std::string& error_message) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    if (ec) {
        error_message = "failed to create parent directory: " + path.parent_path().string();
        return false;
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        error_message = "failed to open file for write: " + path.string();
        return false;
    }

    output << text;
    output.flush();
    if (!output.good()) {
        error_message = "failed to write file: " + path.string();
        return false;
    }
    return true;
}

}  // namespace Common
}  // namespace ProcessInterface
