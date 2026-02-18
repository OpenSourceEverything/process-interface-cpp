#include "port_probe.h"

#include <cstdlib>
#include <string>

namespace ProcessInterface {
namespace Platform {

namespace {

std::string QuoteForShell(const std::string& value) {
    std::string quoted = "\"";
    std::size_t index = 0;
    for (index = 0; index < value.size(); ++index) {
        const char c = value[index];
        if (c == '"') {
            quoted += "\"\"";
        } else {
            quoted.push_back(c);
        }
    }
    quoted += "\"";
    return quoted;
}

}  // namespace

bool CheckPortListening(const std::string& host, int port, int timeout_ms) {
    if (host.empty() || port <= 0 || port > 65535) {
        return false;
    }

    const int timeout_value = timeout_ms > 0 ? timeout_ms : 250;
    const std::string python_code =
        "import socket,sys;"
        "h=sys.argv[1];p=int(sys.argv[2]);t=float(sys.argv[3])/1000.0;"
        "s=socket.socket(socket.AF_INET,socket.SOCK_STREAM);"
        "s.settimeout(t);"
        "rc=s.connect_ex((h,p));"
        "s.close();"
        "sys.exit(0 if rc==0 else 1)";

    std::string command = "python -c ";
    command += QuoteForShell(python_code);
    command += " ";
    command += QuoteForShell(host);
    command += " ";
    command += QuoteForShell(std::to_string(port));
    command += " ";
    command += QuoteForShell(std::to_string(timeout_value));

    const int rc = std::system(command.c_str());
    return rc == 0;
}

}  // namespace Platform
}  // namespace ProcessInterface
