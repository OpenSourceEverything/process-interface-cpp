#include "process_probe.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#endif

#include "../common/file_io.h"
#include "../common/fs_compat.h"
#include "../common/text.h"

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

std::string ToLowerCopy(const std::string& text) {
    std::string lowered;
    lowered.reserve(text.size());
    std::size_t index = 0;
    for (index = 0; index < text.size(); ++index) {
        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(text[index]))));
    }
    return lowered;
}

bool IsDigits(const std::string& text) {
    if (text.empty()) {
        return false;
    }
    std::size_t index = 0;
    for (index = 0; index < text.size(); ++index) {
        const char c = text[index];
        if (c < '0' || c > '9') {
            return false;
        }
    }
    return true;
}

bool RunCommandCapture(const std::string& command, std::string& output_text) {
    std::error_code ec;
    const Common::fs::path temp_dir = Common::fs::temp_directory_path(ec);
    if (ec) {
        return false;
    }

    const long long stamp = static_cast<long long>(std::rand());
    const Common::fs::path output_path = temp_dir / ("gpi-process-probe-" + std::to_string(stamp) + ".txt");
    const std::string shell_command = command + " > " + QuoteForShell(output_path.string()) + " 2>&1";
    const int rc = std::system(shell_command.c_str());

    ProcessInterface::Common::ReadTextFile(output_path, output_text);
    Common::fs::remove(output_path, ec);
    return rc == 0;
}

#ifdef _WIN32
std::string WideToUtf8(const wchar_t* text) {
    if (text == NULL || text[0] == L'\0') {
        return std::string();
    }

    const int required = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
    if (required <= 1) {
        return std::string();
    }

    std::string buffer(static_cast<std::size_t>(required), '\0');
    const int written = WideCharToMultiByte(CP_UTF8, 0, text, -1, &buffer[0], required, NULL, NULL);
    if (written <= 1) {
        return std::string();
    }
    buffer.resize(static_cast<std::size_t>(written - 1));
    return buffer;
}

bool ProbeFromToolhelp(const std::string& process_name, std::vector<int>& pids_out) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    const std::string target = ToLowerCopy(process_name);
    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(entry);
    if (!Process32FirstW(snapshot, &entry)) {
        CloseHandle(snapshot);
        return false;
    }

    do {
        const std::string image_name = ToLowerCopy(WideToUtf8(entry.szExeFile));
        if (image_name != target) {
            entry.dwSize = sizeof(entry);
            continue;
        }

        const DWORD pid = entry.th32ProcessID;
        if (pid > 0 && pid <= static_cast<DWORD>(std::numeric_limits<int>::max())) {
            pids_out.push_back(static_cast<int>(pid));
        }
        entry.dwSize = sizeof(entry);
    } while (Process32NextW(snapshot, &entry));

    CloseHandle(snapshot);
    return true;
}
#endif

void ProbeFromPgrep(const std::string& process_name, std::vector<int>& pids_out) {
    std::string output;
    if (!RunCommandCapture("pgrep -i -x " + QuoteForShell(process_name), output)) {
        return;
    }

    const std::vector<std::string> lines = ProcessInterface::Common::SplitNonEmptyLines(output);
    std::size_t index = 0;
    for (index = 0; index < lines.size(); ++index) {
        const std::string value = ProcessInterface::Common::TrimCopy(lines[index]);
        if (IsDigits(value)) {
            pids_out.push_back(std::atoi(value.c_str()));
        }
    }
}

void ProbeFromTaskList(const std::string& process_name, std::vector<int>& pids_out) {
    std::string output;
    if (!RunCommandCapture("tasklist /FO CSV /NH", output)) {
        return;
    }

    const std::string target = ToLowerCopy(process_name);
    const std::vector<std::string> lines = ProcessInterface::Common::SplitNonEmptyLines(output);
    std::size_t line_index = 0;
    for (line_index = 0; line_index < lines.size(); ++line_index) {
        const std::string line = ProcessInterface::Common::TrimCopy(lines[line_index]);
        if (line.size() < 5 || line[0] != '"') {
            continue;
        }

        const std::size_t name_end = line.find("\",\"");
        if (name_end == std::string::npos) {
            continue;
        }

        const std::string image_name = line.substr(1, name_end - 1);
        if (ToLowerCopy(image_name) != target) {
            continue;
        }

        const std::size_t pid_start = name_end + 3;
        const std::size_t pid_end = line.find("\",\"", pid_start);
        if (pid_end == std::string::npos || pid_end <= pid_start) {
            continue;
        }
        const std::string pid_text = line.substr(pid_start, pid_end - pid_start);
        if (IsDigits(pid_text)) {
            pids_out.push_back(std::atoi(pid_text.c_str()));
        }
    }
}

}  // namespace

ProcessQueryResult QueryProcessByName(const std::string& process_name) {
    ProcessQueryResult result;
    result.running = false;
    result.pid = 0;

    if (process_name.empty()) {
        return result;
    }

    std::vector<int> pids;
#ifdef _WIN32
    ProbeFromToolhelp(process_name, pids);
    if (pids.empty()) {
        ProbeFromTaskList(process_name, pids);
    }
#else
    ProbeFromPgrep(process_name, pids);
#endif

    if (!pids.empty()) {
        std::sort(pids.begin(), pids.end());
        pids.erase(std::unique(pids.begin(), pids.end()), pids.end());
        result.running = true;
        result.pid = pids[0];
        result.pids = pids;
    }

    return result;
}

}  // namespace Platform
}  // namespace ProcessInterface
