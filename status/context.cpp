#include "context.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <sstream>
#include <string>

#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

namespace ProcessInterface {
namespace Status {

namespace {

std::string Trim(const std::string& value) {
    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
        ++start;
    }
    if (start >= value.size()) {
        return std::string();
    }
    std::size_t end = value.size() - 1;
    while (end > start && std::isspace(static_cast<unsigned char>(value[end])) != 0) {
        --end;
    }
    return value.substr(start, end - start + 1);
}

std::vector<std::string> ParseCsvRow(const std::string& line) {
    std::vector<std::string> values;
    std::string current;
    bool in_quotes = false;
    std::size_t index;
    for (index = 0; index < line.size(); ++index) {
        const char c = line[index];
        if (in_quotes) {
            if (c == '"') {
                if ((index + 1) < line.size() && line[index + 1] == '"') {
                    current.push_back('"');
                    ++index;
                } else {
                    in_quotes = false;
                }
            } else {
                current.push_back(c);
            }
            continue;
        }
        if (c == '"') {
            in_quotes = true;
            continue;
        }
        if (c == ',') {
            values.push_back(current);
            current.clear();
            continue;
        }
        current.push_back(c);
    }
    values.push_back(current);
    return values;
}

}  // namespace

ProcessProbeResult QueryProcessByName(const std::string& process_name) {
    ProcessProbeResult result;
    result.running = false;
    result.pid = 0;

    const std::string command =
        "tasklist /FI \"IMAGENAME eq " + process_name + "\" /FO CSV /NH";
    FILE* pipe = _popen(command.c_str(), "r");
    if (pipe == NULL) {
        return result;
    }

    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        const std::string raw_line(buffer);
        const std::string line = Trim(raw_line);
        if (line.empty()) {
            continue;
        }
        if (line.find("No tasks are running") != std::string::npos) {
            continue;
        }
        const std::vector<std::string> row = ParseCsvRow(line);
        if (row.size() < 2) {
            continue;
        }
        std::istringstream parser(row[1]);
        int pid = 0;
        parser >> pid;
        if (pid <= 0) {
            continue;
        }
        result.pids.push_back(pid);
    }
    _pclose(pipe);

    if (!result.pids.empty()) {
        std::sort(result.pids.begin(), result.pids.end());
        result.running = true;
        result.pid = result.pids[0];
    }
    return result;
}

bool CheckPortListening(const std::string& host, int port, int timeout_ms) {
    if (port <= 0 || port > 65535) {
        return false;
    }

    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        return false;
    }

    SOCKET socket_handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_handle == INVALID_SOCKET) {
        WSACleanup();
        return false;
    }

    u_long non_blocking = 1;
    ioctlsocket(socket_handle, FIONBIO, &non_blocking);

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<u_short>(port));
    if (inet_pton(AF_INET, host.c_str(), &address.sin_addr) != 1) {
        closesocket(socket_handle);
        WSACleanup();
        return false;
    }

    const int connect_result = connect(socket_handle, reinterpret_cast<const sockaddr*>(&address), sizeof(address));
    if (connect_result == 0) {
        closesocket(socket_handle);
        WSACleanup();
        return true;
    }

    fd_set write_set;
    FD_ZERO(&write_set);
    FD_SET(socket_handle, &write_set);

    timeval timeout_value;
    timeout_value.tv_sec = timeout_ms / 1000;
    timeout_value.tv_usec = (timeout_ms % 1000) * 1000;

    const int select_result = select(0, NULL, &write_set, NULL, &timeout_value);
    bool success = false;
    if (select_result > 0 && FD_ISSET(socket_handle, &write_set)) {
        int socket_error = 0;
        int socket_error_size = sizeof(socket_error);
        if (getsockopt(
                socket_handle,
                SOL_SOCKET,
                SO_ERROR,
                reinterpret_cast<char*>(&socket_error),
                &socket_error_size) == 0) {
            success = (socket_error == 0);
        }
    }

    closesocket(socket_handle);
    WSACleanup();
    return success;
}

}  // namespace Status
}  // namespace ProcessInterface
