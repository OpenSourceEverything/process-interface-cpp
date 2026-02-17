#ifndef GPI_PROVIDER_API_H
#define GPI_PROVIDER_API_H

#include <string>

namespace gpi {

class StatusProvider {
public:
    virtual ~StatusProvider() {}
    virtual bool ReadStatusJson(const std::string& app_id, std::string& status_json, std::string& error_message) = 0;
    virtual bool GetConfigJson(const std::string& app_id, std::string& config_json, std::string& error_message) = 0;
    virtual bool SetConfigValue(
        const std::string& app_id,
        const std::string& key,
        const std::string& value,
        std::string& result_json,
        std::string& error_message) = 0;
    virtual bool ListActionsJson(const std::string& app_id, std::string& actions_json, std::string& error_message) = 0;
    virtual bool InvokeAction(
        const std::string& app_id,
        const std::string& action_name,
        const std::string& args_json,
        double timeout_seconds,
        std::string& result_json,
        std::string& error_message) = 0;
};

}  // namespace gpi

#endif  // GPI_PROVIDER_API_H
