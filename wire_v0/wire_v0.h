#ifndef GPI_WIRE_V0_H
#define GPI_WIRE_V0_H

#include <string>

namespace gpi {

struct WireRequest {
    std::string request_id;
    std::string method;
    std::string app_id;
    std::string key;
    std::string value;
};

std::string JsonEscape(const std::string& value);
bool ParseRequestLine(const std::string& request_line, WireRequest& request, std::string& error_message);
std::string BuildOkResponse(const std::string& request_id, const std::string& response_json_object);
std::string BuildErrorResponse(
    const std::string& request_id,
    const std::string& error_code,
    const std::string& error_message,
    const std::string& error_details_json_object);

}  // namespace gpi

#endif  // GPI_WIRE_V0_H
