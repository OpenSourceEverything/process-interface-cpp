#include "action_jobs.h"

#include <atomic>
#include <cstdlib>
#include <sstream>

#include "../../../external/nlohmann/json.hpp"
#include "../../common/file_io.h"
#include "../../common/time_utils.h"
#include "../../platform/file_replace.h"

namespace ProcessInterface {
namespace Common {

namespace {

std::atomic<unsigned long long> g_job_counter(0);

nlohmann::json ParseObjectOrDefault(const std::string& text) {
    if (text.empty()) {
        return nlohmann::json::object();
    }

    try {
        const nlohmann::json value = nlohmann::json::parse(text);
        if (value.is_object()) {
            return value;
        }
    } catch (const std::exception&) {
    }

    return nlohmann::json::object();
}

}  // namespace

std::string GenerateJobId() {
    const unsigned long long counter = ++g_job_counter;
    const long long epoch_ms = CurrentEpochMs();

    std::ostringstream stream;
    stream << "job-" << epoch_ms << "-" << counter;
    return stream.str();
}

fs::path ResolveActionJobPath(
    const fs::path& repo_root,
    const Common::PathTemplateSet& path_templates,
    const std::string& app_id,
    const std::string& job_id) {
    const Common::PathTemplateArgs path_args = {
        repo_root.string(),
        app_id,
        job_id,
    };
    return Common::RenderTemplatePath(path_templates.action_job_path, path_args);
}

bool WriteActionJobRecord(
    const fs::path& repo_root,
    const Common::PathTemplateSet& path_templates,
    const std::string& app_id,
    const ActionJobRecord& record,
    std::string& error_message) {
    nlohmann::json root;
    root["jobId"] = record.job_id;
    root["state"] = record.state;
    root["acceptedAt"] = record.accepted_at;
    root["startedAt"] = record.started_at;
    root["finishedAt"] = record.finished_at;
    root["result"] = ParseObjectOrDefault(record.result_json);
    root["stdout"] = record.stdout_text;
    root["stderr"] = record.stderr_text;

    if (record.has_error) {
        nlohmann::json error;
        error["code"] = record.error_code;
        error["message"] = record.error_message;
        error["details"] = nlohmann::json::object();
        root["error"] = error;
    } else {
        root["error"] = nullptr;
    }

    const fs::path path = ResolveActionJobPath(repo_root, path_templates, app_id, record.job_id);
    return ProcessInterface::Platform::AtomicReplaceFile(path, root.dump(), error_message);
}

bool ReadActionJobRecord(
    const fs::path& repo_root,
    const Common::PathTemplateSet& path_templates,
    const std::string& app_id,
    const std::string& job_id,
    ActionJobRecord& record_out,
    std::string& error_message) {
    const fs::path path = ResolveActionJobPath(repo_root, path_templates, app_id, job_id);

    std::string text;
    if (!ReadTextFile(path, text)) {
        error_message = "job not found";
        return false;
    }

    nlohmann::json root;
    try {
        root = nlohmann::json::parse(text);
    } catch (const std::exception&) {
        error_message = "job record is invalid JSON";
        return false;
    }

    if (!root.is_object()) {
        error_message = "job record must be object";
        return false;
    }

    ActionJobRecord record;
    record.job_id = root.value("jobId", std::string());
    record.state = root.value("state", std::string());
    record.accepted_at = root.value("acceptedAt", std::string());
    record.started_at = root.value("startedAt", std::string());
    record.finished_at = root.value("finishedAt", std::string());
    record.stdout_text = root.value("stdout", std::string());
    record.stderr_text = root.value("stderr", std::string());

    if (root.contains("result") && root["result"].is_object()) {
        record.result_json = root["result"].dump();
    } else {
        record.result_json = "{}";
    }

    record.has_error = false;
    record.error_code.clear();
    record.error_message.clear();

    if (root.contains("error") && !root["error"].is_null() && root["error"].is_object()) {
        record.has_error = true;
        record.error_code = root["error"].value("code", std::string());
        record.error_message = root["error"].value("message", std::string());
    }

    if (record.job_id.empty() || record.state.empty()) {
        error_message = "job record missing required fields";
        return false;
    }

    record_out = record;
    return true;
}

}  // namespace Common
}  // namespace ProcessInterface
