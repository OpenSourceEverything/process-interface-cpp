#include "wire_v0.h"

#include <regex>

namespace gpi {

namespace {

bool IsJsonObjectText(const std::string& text) {
    std::string compact;
    compact.reserve(text.size());
    std::size_t index;
    for (index = 0; index < text.size(); ++index) {
        const char c = text[index];
        if (c != '\r' && c != '\n' && c != '\t' && c != ' ') {
            compact.push_back(c);
        }
    }
    if (compact.size() < 2) {
        return false;
    }
    return compact.front() == '{' && compact.back() == '}';
}

bool ExtractStringValue(const std::string& source, const std::string& key, std::string& value_out) {
    const std::string pattern_text = "\"" + key + "\"\\s*:\\s*\"([^\"]*)\"";
    const std::regex pattern(pattern_text);
    std::smatch match;
    if (!std::regex_search(source, match, pattern)) {
        return false;
    }
    if (match.size() < 2) {
        return false;
    }
    value_out = match[1].str();
    return true;
}

}  // namespace

std::string JsonEscape(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() + 16);
    std::size_t index;
    for (index = 0; index < value.size(); ++index) {
        const char c = value[index];
        switch (c) {
            case '\\':
                escaped += "\\\\";
                break;
            case '"':
                escaped += "\\\"";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped.push_back(c);
                break;
        }
    }
    return escaped;
}

bool ParseRequestLine(const std::string& request_line, WireRequest& request, std::string& error_message) {
    if (!IsJsonObjectText(request_line)) {
        error_message = "request is not a JSON object";
        return false;
    }

    request = WireRequest();
    if (!ExtractStringValue(request_line, "id", request.request_id)) {
        request.request_id.clear();
    }
    if (!ExtractStringValue(request_line, "method", request.method)) {
        error_message = "missing required key: method";
        return false;
    }

    std::string app_id;
    if (ExtractStringValue(request_line, "appId", app_id)) {
        request.app_id = app_id;
    }

    std::string key;
    if (ExtractStringValue(request_line, "key", key)) {
        request.key = key;
    }

    std::string value;
    if (ExtractStringValue(request_line, "value", value)) {
        request.value = value;
    }

    return true;
}

std::string BuildOkResponse(const std::string& request_id, const std::string& response_json_object) {
    std::string id_part;
    if (!request_id.empty()) {
        id_part = "\"id\":\"" + JsonEscape(request_id) + "\",";
    }
    return "{" + id_part + "\"ok\":true,\"response\":" + response_json_object + "}";
}

std::string BuildErrorResponse(
    const std::string& request_id,
    const std::string& error_code,
    const std::string& error_message,
    const std::string& error_details_json_object) {
    std::string id_part;
    if (!request_id.empty()) {
        id_part = "\"id\":\"" + JsonEscape(request_id) + "\",";
    }
    return "{"
        + id_part
        + "\"ok\":false,\"error\":{\"code\":\""
        + JsonEscape(error_code)
        + "\",\"message\":\""
        + JsonEscape(error_message)
        + "\",\"details\":"
        + error_details_json_object
        + "}}";
}

}  // namespace gpi
