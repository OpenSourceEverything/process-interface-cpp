#include "api.h"

#include "context.h"
#include "debug.h"
#include "engine.h"
#include "spec_loader.h"
#include "writer.h"

namespace ProcessInterface {
namespace Status {

StatusResult CollectAndPublishStatus(const fs::path& repo_root, const std::string& app_id) {
    StatusResult result;
    result.ok = false;
    result.error_code = StatusErrorCode::kCollectFailed;

    StatusSpec spec;
    std::string error_message;
    StatusErrorCode rc = LoadStatusSpec(repo_root, app_id, spec, error_message);
    if (rc != StatusErrorCode::kNone) {
        result.error_code = rc;
        result.error_message = error_message;
        return result;
    }

    StatusContext context;
    context.app_id = app_id;
    context.repo_root = repo_root;

    std::string payload_json;
    rc = ExecuteStatusSpec(spec, context, payload_json, error_message);
    if (rc != StatusErrorCode::kNone) {
        result.error_code = rc;
        result.error_message = error_message;
        return result;
    }

    rc = WriteSnapshotEnvelope(repo_root, app_id, payload_json, error_message);
    if (rc != StatusErrorCode::kNone) {
        result.error_code = rc;
        result.error_message = error_message;
        return result;
    }

    if (DebugEnabled()) {
        DebugLog("status snapshot written for appId=" + app_id);
    }

    result.ok = true;
    result.error_code = StatusErrorCode::kNone;
    result.payload_json = payload_json;
    return result;
}

}  // namespace Status
}  // namespace ProcessInterface

