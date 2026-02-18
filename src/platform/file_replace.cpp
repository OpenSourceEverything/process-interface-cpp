#include "file_replace.h"

#include <chrono>
#include <fstream>

namespace ProcessInterface {
namespace Platform {

bool AtomicReplaceFile(const Common::fs::path& target_path, const std::string& contents, std::string& error_message) {
    std::error_code ec;
    Common::fs::create_directories(target_path.parent_path(), ec);
    if (ec) {
        error_message = "failed to create parent directory: " + target_path.parent_path().string();
        return false;
    }

    const long long stamp = std::chrono::system_clock::now().time_since_epoch().count();
    const Common::fs::path temp_path =
        target_path.parent_path() / (target_path.filename().string() + ".tmp." + std::to_string(stamp));

    {
        std::ofstream output(temp_path, std::ios::binary | std::ios::trunc);
        if (!output.is_open()) {
            error_message = "failed to open temp file: " + temp_path.string();
            return false;
        }
        output << contents;
        output.flush();
        if (!output.good()) {
            error_message = "failed to write temp file: " + temp_path.string();
            output.close();
            Common::fs::remove(temp_path, ec);
            return false;
        }
    }

    Common::fs::rename(temp_path, target_path, ec);
    if (!ec) {
        return true;
    }

    ec.clear();
    Common::fs::remove(target_path, ec);
    ec.clear();
    Common::fs::rename(temp_path, target_path, ec);
    if (ec) {
        error_message = "failed to replace file: " + target_path.string();
        Common::fs::remove(temp_path, ec);
        return false;
    }

    return true;
}

}  // namespace Platform
}  // namespace ProcessInterface
