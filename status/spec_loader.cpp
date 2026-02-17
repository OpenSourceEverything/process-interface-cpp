#include "spec_loader.h"

#include <cctype>
#include <fstream>
#include <regex>
#include <sstream>

#include "paths.h"

namespace ProcessInterface {
namespace Status {

namespace {

bool ReadTextFile(const fs::path& path, std::string& text_out) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return false;
    }
    std::ostringstream stream;
    stream << input.rdbuf();
    text_out = stream.str();
    return true;
}

bool ExtractStringValue(const std::string& source, const std::string& key, std::string& value_out) {
    const std::string pattern_text = "\"" + key + "\"\\s*:\\s*\"((?:[^\"\\\\]|\\\\.)*)\"";
    const std::regex pattern(pattern_text);
    std::smatch match;
    if (!std::regex_search(source, match, pattern)) {
        return false;
    }
    if (match.size() < 2) {
        return false;
    }
    std::string value = match[1].str();
    std::string unescaped;
    unescaped.reserve(value.size());
    bool escaped = false;
    std::size_t index;
    for (index = 0; index < value.size(); ++index) {
        const char c = value[index];
        if (escaped) {
            switch (c) {
                case 'n':
                    unescaped.push_back('\n');
                    break;
                case 'r':
                    unescaped.push_back('\r');
                    break;
                case 't':
                    unescaped.push_back('\t');
                    break;
                default:
                    unescaped.push_back(c);
                    break;
            }
            escaped = false;
            continue;
        }
        if (c == '\\') {
            escaped = true;
            continue;
        }
        unescaped.push_back(c);
    }
    value_out = unescaped;
    return true;
}

bool ExtractArrayBlock(const std::string& source, const std::string& key, std::string& block_out) {
    const std::string pattern_text = "\"" + key + "\"\\s*:\\s*\\[";
    const std::regex pattern(pattern_text);
    std::smatch match;
    if (!std::regex_search(source, match, pattern)) {
        return false;
    }
    std::size_t index = static_cast<std::size_t>(match.position(0) + match.length(0));
    std::size_t start = index;
    int depth = 1;
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
        if (c == '[') {
            ++depth;
            continue;
        }
        if (c == ']') {
            --depth;
            if (depth == 0) {
                block_out = source.substr(start, index - start);
                return true;
            }
        }
    }
    return false;
}

std::vector<std::string> ParseStringItems(const std::string& block) {
    std::vector<std::string> items;
    std::size_t index = 0;
    while (index < block.size()) {
        while (index < block.size() && std::isspace(static_cast<unsigned char>(block[index])) != 0) {
            ++index;
        }
        if (index >= block.size()) {
            break;
        }
        if (block[index] != '"') {
            ++index;
            continue;
        }
        ++index;
        std::string value;
        bool escaped = false;
        while (index < block.size()) {
            const char c = block[index];
            ++index;
            if (escaped) {
                switch (c) {
                    case 'n':
                        value.push_back('\n');
                        break;
                    case 'r':
                        value.push_back('\r');
                        break;
                    case 't':
                        value.push_back('\t');
                        break;
                    default:
                        value.push_back(c);
                        break;
                }
                escaped = false;
                continue;
            }
            if (c == '\\') {
                escaped = true;
                continue;
            }
            if (c == '"') {
                break;
            }
            value.push_back(c);
        }
        items.push_back(value);
    }
    return items;
}

}  // namespace

StatusErrorCode LoadStatusSpec(
    const fs::path& repo_root,
    const std::string& app_id,
    StatusSpec& spec_out,
    std::string& error_message) {
    const fs::path spec_path = ResolveSpecPath(repo_root, app_id);
    std::string spec_text;
    if (!ReadTextFile(spec_path, spec_text)) {
        error_message = "status spec file not found: " + spec_path.string();
        return StatusErrorCode::kSpecMissing;
    }

    StatusSpec spec;
    spec.app_id = app_id;
    if (!ExtractStringValue(spec_text, "appId", spec.app_id)) {
        spec.app_id = app_id;
    }
    if (spec.app_id != app_id) {
        error_message = "status spec appId mismatch for " + app_id;
        return StatusErrorCode::kSpecInvalid;
    }
    if (!ExtractStringValue(spec_text, "appTitle", spec.app_title)) {
        error_message = "status spec missing appTitle: " + spec_path.string();
        return StatusErrorCode::kSpecInvalid;
    }

    if (!ExtractStringValue(spec_text, "runningField", spec.running_field)) {
        spec.running_field = "running";
    }
    if (!ExtractStringValue(spec_text, "pidField", spec.pid_field)) {
        spec.pid_field = "pid";
    }
    if (!ExtractStringValue(spec_text, "hostRunningField", spec.host_running_field)) {
        spec.host_running_field = spec.running_field;
    }
    if (!ExtractStringValue(spec_text, "hostPidField", spec.host_pid_field)) {
        spec.host_pid_field = spec.pid_field;
    }

    std::string operations_block;
    if (!ExtractArrayBlock(spec_text, "operations", operations_block)) {
        error_message = "status spec missing operations array: " + spec_path.string();
        return StatusErrorCode::kSpecInvalid;
    }
    spec.operations = ParseStringItems(operations_block);
    if (spec.operations.empty()) {
        error_message = "status spec operations empty: " + spec_path.string();
        return StatusErrorCode::kSpecInvalid;
    }

    spec_out = spec;
    return StatusErrorCode::kNone;
}

}  // namespace Status
}  // namespace ProcessInterface

