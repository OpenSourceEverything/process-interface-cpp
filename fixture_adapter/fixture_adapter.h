#ifndef GPI_FIXTURE_ADAPTER_H
#define GPI_FIXTURE_ADAPTER_H

#include <string>

#include "../provider_api/provider_api.h"

namespace gpi {

class FixtureStatusAdapter : public StatusProvider {
public:
    explicit FixtureStatusAdapter(const std::string& fixture_repo_root);
    bool ReadStatusJson(const std::string& app_id, std::string& status_json, std::string& error_message);

private:
    std::string fixture_repo_root_;
};

}  // namespace gpi

#endif  // GPI_FIXTURE_ADAPTER_H
