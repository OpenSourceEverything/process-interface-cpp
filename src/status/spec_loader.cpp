#include "spec_loader.h"

#include "../../external/nlohmann/json.hpp"
#include "../common/file_io.h"
#include "paths.h"

namespace ProcessInterface {
namespace Status {

StatusErrorCode LoadStatusSpec(
    const fs::path& repo_root,
    const Common::PathTemplateSet& path_templates,
    const std::string& app_id,
    StatusSpec& spec_out,
    std::string& error_message) {
    const fs::path spec_path = ResolveSpecPath(repo_root, path_templates, app_id);

    std::string spec_text;
    if (!ProcessInterface::Common::ReadTextFile(spec_path, spec_text)) {
        error_message = "status spec file not found: " + spec_path.string();
        return StatusErrorCode::kSpecMissing;
    }

    nlohmann::json root;
    try {
        root = nlohmann::json::parse(spec_text);
    } catch (const std::exception&) {
        error_message = "status spec is not valid JSON: " + spec_path.string();
        return StatusErrorCode::kSpecInvalid;
    }

    if (!root.is_object()) {
        error_message = "status spec must be JSON object: " + spec_path.string();
        return StatusErrorCode::kSpecInvalid;
    }

    StatusSpec spec;
    spec.app_id = app_id;

    if (root.contains("appId") && root["appId"].is_string()) {
        spec.app_id = root["appId"].get<std::string>();
    }
    if (spec.app_id != app_id) {
        error_message = "status spec appId mismatch for " + app_id;
        return StatusErrorCode::kSpecInvalid;
    }

    if (!root.contains("appTitle") || !root["appTitle"].is_string()) {
        error_message = "status spec missing appTitle: " + spec_path.string();
        return StatusErrorCode::kSpecInvalid;
    }
    spec.app_title = root["appTitle"].get<std::string>();

    spec.running_field = root.value("runningField", std::string("running"));
    spec.pid_field = root.value("pidField", std::string("pid"));
    spec.host_running_field = root.value("hostRunningField", spec.running_field);
    spec.host_pid_field = root.value("hostPidField", spec.pid_field);

    if (!root.contains("operations") || !root["operations"].is_array()) {
        error_message = "status spec missing operations array: " + spec_path.string();
        return StatusErrorCode::kSpecInvalid;
    }

    spec.operations.clear();
    const nlohmann::json& operations = root["operations"];

    std::size_t index;
    for (index = 0; index < operations.size(); ++index) {
        const nlohmann::json& op = operations[index];
        if (op.is_string()) {
            ParsedOperation parsed;
            std::string parse_error;
            if (!ParseStatusExpressionLine(op.get<std::string>(), parsed, parse_error)) {
                error_message = "status spec operation parse failed: " + parse_error;
                return StatusErrorCode::kSpecInvalid;
            }
            spec.operations.push_back(parsed);
        }
    }

    if (spec.operations.empty()) {
        error_message = "status spec operations empty: " + spec_path.string();
        return StatusErrorCode::kSpecInvalid;
    }

    spec_out = spec;
    return StatusErrorCode::kNone;
}

}  // namespace Status
}  // namespace ProcessInterface
