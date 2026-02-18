#ifndef PROCESS_INTERFACE_STATUS_EXPRESSION_PARSER_H
#define PROCESS_INTERFACE_STATUS_EXPRESSION_PARSER_H

#include <string>
#include <vector>

namespace ProcessInterface {
namespace Status {

struct ParsedOperation {
    std::string field_name;
    std::string op_name;
    std::vector<std::string> args;
};

bool ParseStatusExpressionLine(
    const std::string& line,
    ParsedOperation& operation_out,
    std::string& error_message);

}  // namespace Status
}  // namespace ProcessInterface

#endif  // PROCESS_INTERFACE_STATUS_EXPRESSION_PARSER_H
