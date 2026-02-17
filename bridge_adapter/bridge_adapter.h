#ifndef GPI_BRIDGE_ADAPTER_H
#define GPI_BRIDGE_ADAPTER_H

#include <string>

#include "../provider_api/provider_api.h"

namespace gpi {

class BridgeStatusAdapter : public StatusProvider {
public:
    explicit BridgeStatusAdapter(const std::string& bridge_repo_root);
    bool ReadStatusJson(const std::string& app_id, std::string& status_json, std::string& error_message) override;
    bool GetConfigJson(const std::string& app_id, std::string& config_json, std::string& error_message) override;
    bool SetConfigValue(
        const std::string& app_id,
        const std::string& key,
        const std::string& value,
        std::string& result_json,
        std::string& error_message) override;
    bool ListActionsJson(const std::string& app_id, std::string& actions_json, std::string& error_message) override;
    bool InvokeAction(
        const std::string& app_id,
        const std::string& action_name,
        const std::string& args_json,
        double timeout_seconds,
        std::string& result_json,
        std::string& error_message) override;

private:
    std::string bridge_repo_root_;
};

}  // namespace gpi

#endif  // GPI_BRIDGE_ADAPTER_H
