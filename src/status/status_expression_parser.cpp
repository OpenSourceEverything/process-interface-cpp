#include "status_expression_parser.h"

#include "../common/text.h"

namespace ProcessInterface {
namespace Status {

bool ParseStatusExpressionLine(
    const std::string& line,
    ParsedOperation& operation_out,
    std::string& error_message) {
    const std::string trimmed_line = ProcessInterface::Common::TrimCopy(line);
    if (trimmed_line.empty()) {
        error_message = "operation line is empty";
        return false;
    }

    const std::size_t equal_index = trimmed_line.find('=');
    if (equal_index == std::string::npos || equal_index == 0 || equal_index + 1 >= trimmed_line.size()) {
        error_message = "invalid operation line: " + trimmed_line;
        return false;
    }

    ParsedOperation operation;
    operation.field_name = ProcessInterface::Common::TrimCopy(trimmed_line.substr(0, equal_index));
    if (operation.field_name.empty()) {
        error_message = "operation field is empty";
        return false;
    }

    const std::string expression = ProcessInterface::Common::TrimCopy(trimmed_line.substr(equal_index + 1));
    const std::vector<std::string> parts = ProcessInterface::Common::Split(expression, ':');
    if (parts.empty()) {
        error_message = "operation expression is empty";
        return false;
    }

    operation.op_name = ProcessInterface::Common::TrimCopy(parts[0]);
    if (operation.op_name.empty()) {
        error_message = "operation name is empty";
        return false;
    }

    std::size_t part_index = 0;
    for (part_index = 1; part_index < parts.size(); ++part_index) {
        operation.args.push_back(parts[part_index]);
    }

    operation_out = operation;
    return true;
}

}  // namespace Status
}  // namespace ProcessInterface
