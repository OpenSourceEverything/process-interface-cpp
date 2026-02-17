#ifndef GPI_PROVIDER_API_H
#define GPI_PROVIDER_API_H

#include <string>

namespace gpi {

class StatusProvider {
public:
    virtual ~StatusProvider() {}
    virtual bool ReadStatusJson(const std::string& app_id, std::string& status_json, std::string& error_message) = 0;
};

}  // namespace gpi

#endif  // GPI_PROVIDER_API_H
