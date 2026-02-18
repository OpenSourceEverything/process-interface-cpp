#include "action_catalog.h"

#include "../../../external/nlohmann/json.hpp"
#include "../../common/file_io.h"

namespace ProcessInterface {
namespace Common {

bool LoadActionCatalog(
    const fs::path& repo_root,
    const Common::PathTemplateSet& path_templates,
    const std::string& app_id,
    std::vector<ActionDefinition>& actions_out,
    std::string& error_message) {
    const Common::PathTemplateArgs path_args = {
        repo_root.string(),
        app_id,
        std::string(),
    };
    const fs::path catalog_path =
        Common::RenderTemplatePath(path_templates.action_catalog_path, path_args);

    std::string catalog_text;
    if (!ReadTextFile(catalog_path, catalog_text)) {
        error_message = "action catalog file not found: " + catalog_path.string();
        return false;
    }

    nlohmann::json root;
    try {
        root = nlohmann::json::parse(catalog_text);
    } catch (const std::exception&) {
        error_message = "action catalog is not valid JSON: " + catalog_path.string();
        return false;
    }

    if (!root.is_object() || !root.contains("actions") || !root["actions"].is_array()) {
        error_message = "action catalog missing actions array: " + catalog_path.string();
        return false;
    }

    actions_out.clear();
    const nlohmann::json& actions_json = root["actions"];

    std::size_t index;
    for (index = 0; index < actions_json.size(); ++index) {
        const nlohmann::json& item = actions_json[index];
        if (!item.is_object()) {
            continue;
        }

        if (!item.contains("name") || !item["name"].is_string()) {
            continue;
        }
        if (!item.contains("cmd") || !item["cmd"].is_array()) {
            continue;
        }

        ActionDefinition action;
        action.name = item["name"].get<std::string>();
        if (action.name.empty()) {
            continue;
        }

        if (item.contains("label") && item["label"].is_string()) {
            action.label = item["label"].get<std::string>();
        }
        if (action.label.empty()) {
            action.label = action.name;
        }

        action.command.clear();
        const nlohmann::json& cmd = item["cmd"];
        std::size_t cmd_index;
        for (cmd_index = 0; cmd_index < cmd.size(); ++cmd_index) {
            const nlohmann::json& token = cmd[cmd_index];
            if (token.is_string()) {
                action.command.push_back(token.get<std::string>());
            }
        }
        if (action.command.empty()) {
            continue;
        }

        action.cwd.clear();
        if (item.contains("cwd") && item["cwd"].is_string()) {
            action.cwd = item["cwd"].get<std::string>();
        }

        action.timeout_seconds = 30.0;
        if (item.contains("timeoutSeconds") && item["timeoutSeconds"].is_number()) {
            const double timeout_value = item["timeoutSeconds"].get<double>();
            if (timeout_value > 0.0) {
                action.timeout_seconds = timeout_value;
            }
        }

        action.detached = false;
        if (item.contains("detached") && item["detached"].is_boolean()) {
            action.detached = item["detached"].get<bool>();
        }

        if (item.contains("args") && item["args"].is_array()) {
            action.args_json = item["args"].dump();
        } else {
            action.args_json = "[]";
        }

        actions_out.push_back(action);
    }

    if (actions_out.empty()) {
        error_message = "action catalog has no runnable actions: " + catalog_path.string();
        return false;
    }

    return true;
}

}  // namespace Common
}  // namespace ProcessInterface
