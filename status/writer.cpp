#include "writer.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>

#include <windows.h>

#include "paths.h"

namespace ProcessInterface {
namespace Status {

namespace {

std::string JsonEscape(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() + 16);
    std::size_t index;
    for (index = 0; index < value.size(); ++index) {
        const char c = value[index];
        switch (c) {
            case '\\':
                escaped += "\\\\";
                break;
            case '"':
                escaped += "\\\"";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped.push_back(c);
                break;
        }
    }
    return escaped;
}

std::string CurrentUtcIso8601() {
    std::time_t now = std::time(NULL);
    std::tm utc_tm;
#if defined(_WIN32)
    gmtime_s(&utc_tm, &now);
#else
    gmtime_r(&now, &utc_tm);
#endif
    char buffer[64];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc_tm);
    return std::string(buffer);
}

bool AtomicReplaceFile(const fs::path& target_path, const std::string& contents, std::string& error_message) {
    fs::create_directories(target_path.parent_path());

    const long long epoch_ticks = std::chrono::system_clock::now().time_since_epoch().count();
    const fs::path temp_path =
        target_path.parent_path() / (target_path.filename().string() + ".tmp." + std::to_string(epoch_ticks));

    {
        std::ofstream output(temp_path, std::ios::binary | std::ios::trunc);
        if (!output.is_open()) {
            error_message = "failed to open temp snapshot file: " + temp_path.string();
            return false;
        }
        output << contents;
        output.flush();
        if (!output.good()) {
            error_message = "failed to write temp snapshot file: " + temp_path.string();
            output.close();
            std::error_code ignored;
            fs::remove(temp_path, ignored);
            return false;
        }
    }

    const std::wstring temp_w = temp_path.wstring();
    const std::wstring target_w = target_path.wstring();
    const BOOL replaced = MoveFileExW(
        temp_w.c_str(),
        target_w.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    if (!replaced) {
        const DWORD win_error = GetLastError();
        std::error_code ignored;
        fs::remove(temp_path, ignored);
        std::ostringstream stream;
        stream << "failed to replace snapshot file: " << target_path.string() << " winerr=" << win_error;
        error_message = stream.str();
        return false;
    }

    return true;
}

}  // namespace

StatusErrorCode WriteSnapshotEnvelope(
    const fs::path& repo_root,
    const std::string& app_id,
    const std::string& payload_json,
    std::string& error_message) {
    const std::string payload = payload_json.empty() ? "{}" : payload_json;
    if (!(payload.front() == '{' && payload.back() == '}')) {
        error_message = "snapshot payload must be JSON object";
        return StatusErrorCode::kSnapshotWriteFailed;
    }

    const long long epoch_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::ostringstream envelope_stream;
    envelope_stream << "{";
    envelope_stream << "\"appId\":\"" << JsonEscape(app_id) << "\",";
    envelope_stream << "\"generatedAt\":\"" << JsonEscape(CurrentUtcIso8601()) << "\",";
    envelope_stream << "\"generatedAtEpochMs\":" << epoch_ms << ",";
    envelope_stream << "\"payload\":" << payload;
    envelope_stream << "}";

    const fs::path snapshot_path = ResolveSnapshotPath(repo_root, app_id);
    if (!AtomicReplaceFile(snapshot_path, envelope_stream.str(), error_message)) {
        return StatusErrorCode::kSnapshotWriteFailed;
    }
    return StatusErrorCode::kNone;
}

}  // namespace Status
}  // namespace ProcessInterface

