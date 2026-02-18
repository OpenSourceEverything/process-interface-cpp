#include "host_profile.h"

#include <string>

#include "../../external/nlohmann/json.hpp"
#include "../common/file_io.h"

namespace ProcessInterface {
namespace HostRuntime {

namespace {

bool RequireString(
    const nlohmann::json& root,
    const std::string& key,
    std::string& value_out,
    const std::string& profile_path,
    std::string& error_message) {
    if (!root.contains(key) || !root[key].is_string()) {
        error_message = "host profile missing string key '" + key + "': " + profile_path;
        return false;
    }
    value_out = root[key].get<std::string>();
    if (value_out.empty()) {
        error_message = "host profile has empty string key '" + key + "': " + profile_path;
        return false;
    }
    return true;
}

}  // namespace

bool LoadHostProfile(
    const Common::fs::path& profile_path,
    HostProfile& profile_out,
    std::string& error_message) {
    std::string text;
    if (!ProcessInterface::Common::ReadTextFile(profile_path, text)) {
        error_message = "host profile not found: " + profile_path.string();
        return false;
    }

    nlohmann::json root;
    try {
        root = nlohmann::json::parse(text);
    } catch (const std::exception&) {
        error_message = "host profile is not valid JSON: " + profile_path.string();
        return false;
    }

    if (!root.is_object()) {
        error_message = "host profile must be JSON object: " + profile_path.string();
        return false;
    }

    HostProfile profile;

    if (!root.contains("allowedApps") || !root["allowedApps"].is_array()) {
        error_message = "host profile missing allowedApps array: " + profile_path.string();
        return false;
    }

    std::size_t index = 0;
    for (index = 0; index < root["allowedApps"].size(); ++index) {
        const nlohmann::json& entry = root["allowedApps"][index];
        if (entry.is_string()) {
            const std::string app_id = entry.get<std::string>();
            if (!app_id.empty()) {
                profile.allowed_apps.push_back(app_id);
            }
        }
    }
    if (profile.allowed_apps.empty()) {
        error_message = "host profile has no valid allowedApps values: " + profile_path.string();
        return false;
    }

    if (!root.contains("paths") || !root["paths"].is_object()) {
        error_message = "host profile missing paths object: " + profile_path.string();
        return false;
    }

    const nlohmann::json& paths = root["paths"];
    if (!RequireString(paths, "statusSpec", profile.path_templates.status_spec_path, profile_path.string(), error_message)) {
        return false;
    }
    if (!RequireString(paths, "statusSnapshot", profile.path_templates.status_snapshot_path, profile_path.string(), error_message)) {
        return false;
    }
    if (!RequireString(paths, "actionCatalog", profile.path_templates.action_catalog_path, profile_path.string(), error_message)) {
        return false;
    }
    if (!RequireString(paths, "actionJob", profile.path_templates.action_job_path, profile_path.string(), error_message)) {
        return false;
    }

    if (!root.contains("ipc") || !root["ipc"].is_object()) {
        error_message = "host profile missing ipc object: " + profile_path.string();
        return false;
    }

    const nlohmann::json& ipc = root["ipc"];
    if (!RequireString(ipc, "backend", profile.ipc.backend, profile_path.string(), error_message)) {
        return false;
    }
    if (!RequireString(ipc, "endpoint", profile.ipc.endpoint, profile_path.string(), error_message)) {
        return false;
    }

    if (profile.ipc.backend != "zmq") {
        error_message = "unsupported ipc.backend in host profile: " + profile.ipc.backend;
        return false;
    }

    if (!ProcessInterface::Common::ValidateTemplateHasToken(
            profile.path_templates.status_spec_path,
            "repoRoot",
            error_message) ||
        !ProcessInterface::Common::ValidateTemplateHasToken(
            profile.path_templates.status_spec_path,
            "appId",
            error_message)) {
        error_message += " (statusSpec)";
        return false;
    }

    if (!ProcessInterface::Common::ValidateTemplateHasToken(
            profile.path_templates.status_snapshot_path,
            "repoRoot",
            error_message) ||
        !ProcessInterface::Common::ValidateTemplateHasToken(
            profile.path_templates.status_snapshot_path,
            "appId",
            error_message)) {
        error_message += " (statusSnapshot)";
        return false;
    }

    if (!ProcessInterface::Common::ValidateTemplateHasToken(
            profile.path_templates.action_catalog_path,
            "repoRoot",
            error_message) ||
        !ProcessInterface::Common::ValidateTemplateHasToken(
            profile.path_templates.action_catalog_path,
            "appId",
            error_message)) {
        error_message += " (actionCatalog)";
        return false;
    }

    if (!ProcessInterface::Common::ValidateTemplateHasToken(
            profile.path_templates.action_job_path,
            "repoRoot",
            error_message) ||
        !ProcessInterface::Common::ValidateTemplateHasToken(
            profile.path_templates.action_job_path,
            "appId",
            error_message) ||
        !ProcessInterface::Common::ValidateTemplateHasToken(
            profile.path_templates.action_job_path,
            "jobId",
            error_message)) {
        error_message += " (actionJob)";
        return false;
    }

    profile_out = profile;
    return true;
}

}  // namespace HostRuntime
}  // namespace ProcessInterface
