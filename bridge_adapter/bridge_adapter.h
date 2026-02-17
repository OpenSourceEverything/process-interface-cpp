#ifndef GPI_BRIDGE_ADAPTER_H
#define GPI_BRIDGE_ADAPTER_H

#include <string>

#include "../provider_api/provider_api.h"

namespace gpi {

class BridgeStatusAdapter : public StatusProvider {
public:
    explicit BridgeStatusAdapter(const std::string& bridge_repo_root);
    bool ReadStatusJson(const std::string& app_id, std::string& status_json, std::string& error_message);

private:
    std::string bridge_repo_root_;
};

}  // namespace gpi

#endif  // GPI_BRIDGE_ADAPTER_H
