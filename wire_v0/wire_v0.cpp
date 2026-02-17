#include "wire_v0.h"

#include <cctype>
#include <cstdlib>
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

bool FindKeyValueStart(const std::string& source, const std::string& key, std::size_t& value_start) {
    const std::string pattern_text = "\"" + key + "\"\\s*:\\s*";
    const std::regex pattern(pattern_text);
    std::smatch match;
    if (!std::regex_search(source, match, pattern)) {
        return false;
    }
    value_start = static_cast<std::size_t>(match.position(0) + match.length(0));
    return true;
}

void SkipWhitespace(const std::string& source, std::size_t& index) {
    while (index < source.size() && std::isspace(static_cast<unsigned char>(source[index])) != 0) {
        ++index;
    }
}

bool ExtractObjectValue(const std::string& source, const std::string& key, std::string& value_out) {
    std::size_t index = 0;
    if (!FindKeyValueStart(source, key, index)) {
        return false;
    }

    SkipWhitespace(source, index);
    if (index >= source.size() || source[index] != '{') {
        return false;
    }

    std::size_t object_start = index;
    std::size_t depth = 0;
    bool in_string = false;
    bool escaped = false;
    for (; index < source.size(); ++index) {
        const char c = source[index];
        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                in_string = false;
            }
            continue;
        }
        if (c == '"') {
            in_string = true;
            continue;
        }
        if (c == '{') {
            ++depth;
            continue;
        }
        if (c == '}') {
            if (depth == 0) {
                return false;
            }
            --depth;
            if (depth == 0) {
                value_out = source.substr(object_start, index - object_start + 1);
                return true;
            }
            continue;
        }
    }
    return false;
}

bool ExtractDoubleValue(const std::string& source, const std::string& key, double& value_out) {
    std::size_t index = 0;
    if (!FindKeyValueStart(source, key, index)) {
        return false;
    }
    SkipWhitespace(source, index);
    if (index >= source.size()) {
        return false;
    }

    std::size_t end = index;
    if (source[end] == '-') {
        ++end;
    }
    bool has_digit = false;
    while (end < source.size() && std::isdigit(static_cast<unsigned char>(source[end])) != 0) {
        has_digit = true;
        ++end;
    }
    if (end < source.size() && source[end] == '.') {
        ++end;
        while (end < source.size() && std::isdigit(static_cast<unsigned char>(source[end])) != 0) {
            has_digit = true;
            ++end;
        }
    }
    if (!has_digit) {
        return false;
    }

    const std::string token = source.substr(index, end - index);
    char* parse_end = NULL;
    const double parsed = std::strtod(token.c_str(), &parse_end);
    if (parse_end == token.c_str()) {
        return false;
    }
    value_out = parsed;
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
    request.args_json = "{}";
    request.timeout_seconds = 0.0;
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

    std::string action_name;
    if (ExtractStringValue(request_line, "actionName", action_name)) {
        request.action_name = action_name;
    }

    std::string job_id;
    if (ExtractStringValue(request_line, "jobId", job_id)) {
        request.job_id = job_id;
    }

    std::string args_json;
    if (ExtractObjectValue(request_line, "args", args_json)) {
        request.args_json = args_json;
    }

    double timeout_seconds = 0.0;
    if (ExtractDoubleValue(request_line, "timeoutSeconds", timeout_seconds) && timeout_seconds > 0.0) {
        request.timeout_seconds = timeout_seconds;
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
