#include "writer.h"

#include "../../external/nlohmann/json.hpp"
#include "../common/time_utils.h"
#include "../platform/file_replace.h"
#include "paths.h"

namespace ProcessInterface {
namespace Status {

StatusErrorCode WriteSnapshotEnvelope(
    const fs::path& repo_root,
    const Common::PathTemplateSet& path_templates,
    const std::string& app_id,
    const std::string& payload_json,
    std::string& error_message) {
    nlohmann::json payload;
    try {
        payload = nlohmann::json::parse(payload_json.empty() ? "{}" : payload_json);
    } catch (const std::exception&) {
        error_message = "snapshot payload must be JSON object";
        return StatusErrorCode::kSnapshotWriteFailed;
    }

    if (!payload.is_object()) {
        error_message = "snapshot payload must be JSON object";
        return StatusErrorCode::kSnapshotWriteFailed;
    }

    nlohmann::json envelope;
    envelope["appId"] = app_id;
    envelope["generatedAt"] = ProcessInterface::Common::CurrentUtcIso8601();
    envelope["generatedAtEpochMs"] = ProcessInterface::Common::CurrentEpochMs();
    envelope["payload"] = payload;

    const fs::path snapshot_path = ResolveSnapshotPath(repo_root, path_templates, app_id);
    if (!ProcessInterface::Platform::AtomicReplaceFile(snapshot_path, envelope.dump(), error_message)) {
        return StatusErrorCode::kSnapshotWriteFailed;
    }

    return StatusErrorCode::kNone;
}

}  // namespace Status
}  // namespace ProcessInterface
