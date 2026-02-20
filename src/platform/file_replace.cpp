#include "file_replace.h"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
#define PROCESS_INTERFACE_PLATFORM_WINDOWS 1
#else
#define PROCESS_INTERFACE_PLATFORM_WINDOWS 0
#endif

#if PROCESS_INTERFACE_PLATFORM_WINDOWS
#include <io.h>
#include <process.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace ProcessInterface {
namespace Platform {

namespace {

std::atomic<unsigned long long> g_temp_counter(0);

#if PROCESS_INTERFACE_PLATFORM_WINDOWS
extern "C" __declspec(dllimport) int __stdcall MoveFileExW(
    const wchar_t* existing_file_name,
    const wchar_t* new_file_name,
    unsigned long flags);

static constexpr unsigned long kMoveFileReplaceExisting = 0x00000001UL;
static constexpr unsigned long kMoveFileWriteThrough = 0x00000008UL;
#endif

int CurrentProcessId() {
#if PROCESS_INTERFACE_PLATFORM_WINDOWS
    return static_cast<int>(::_getpid());
#else
    return static_cast<int>(::getpid());
#endif
}

Common::fs::path BuildTempPath(const Common::fs::path& target_path) {
    const long long tick = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const unsigned long long counter = ++g_temp_counter;
    const unsigned long long tid_hash = static_cast<unsigned long long>(
        std::hash<std::thread::id>()(std::this_thread::get_id()));
    const int pid = CurrentProcessId();

    const std::string suffix = ".tmp."
        + std::to_string(pid) + "."
        + std::to_string(counter) + "."
        + std::to_string(tick) + "."
        + std::to_string(tid_hash);

    return target_path.parent_path() / (target_path.filename().string() + suffix);
}

bool FlushFileDurable(FILE* file, std::string& error_message) {
    if (file == NULL) {
        error_message = "invalid temp file handle";
        return false;
    }

    if (std::fflush(file) != 0) {
        error_message = "failed to flush temp file stream";
        return false;
    }

#if PROCESS_INTERFACE_PLATFORM_WINDOWS
    const int fd = ::_fileno(file);
    if (fd < 0) {
        error_message = "failed to get temp file descriptor";
        return false;
    }
    if (::_commit(fd) != 0) {
        error_message = "failed to commit temp file";
        return false;
    }
#else
    const int fd = ::fileno(file);
    if (fd < 0) {
        error_message = "failed to get temp file descriptor";
        return false;
    }
    if (::fsync(fd) != 0) {
        error_message = "failed to fsync temp file";
        return false;
    }
#endif

    return true;
}

bool WriteTempFileDurable(const Common::fs::path& temp_path,
                          const std::string& contents,
                          std::string& error_message) {
    FILE* file = std::fopen(temp_path.string().c_str(), "wb");
    if (file == NULL) {
        error_message = "failed to open temp file: " + temp_path.string();
        return false;
    }

    bool ok = true;
    if (!contents.empty()) {
        const std::size_t wrote = std::fwrite(contents.data(), 1, contents.size(), file);
        if (wrote != contents.size()) {
            ok = false;
            error_message = "failed to write temp file: " + temp_path.string();
        }
    }

    if (ok && !FlushFileDurable(file, error_message)) {
        ok = false;
    }

    const int close_rc = std::fclose(file);
    if (ok && close_rc != 0) {
        ok = false;
        error_message = "failed to close temp file: " + temp_path.string();
    }

    if (!ok) {
        std::error_code remove_ec;
        Common::fs::remove(temp_path, remove_ec);
        return false;
    }

    return true;
}

bool SyncParentDirectory(const Common::fs::path& target_path, std::string& error_message) {
#if PROCESS_INTERFACE_PLATFORM_WINDOWS
    (void)target_path;
    (void)error_message;
    return true;
#else
    int flags = O_RDONLY;
#if defined(O_DIRECTORY)
    flags |= O_DIRECTORY;
#endif

    const int dir_fd = ::open(target_path.parent_path().string().c_str(), flags);
    if (dir_fd < 0) {
        if (errno == EINVAL) {
            return true;
        }
        error_message = "failed to open parent directory for fsync: " + target_path.parent_path().string();
        return false;
    }

    const int sync_rc = ::fsync(dir_fd);
    const int close_rc = ::close(dir_fd);
    if (sync_rc != 0) {
        if (errno == EINVAL) {
            return true;
        }
        error_message = "failed to fsync parent directory: " + target_path.parent_path().string();
        return false;
    }
    if (close_rc != 0) {
        error_message = "failed to close parent directory handle: " + target_path.parent_path().string();
        return false;
    }
    return true;
#endif
}

bool ReplaceTargetWithTemp(const Common::fs::path& temp_path,
                           const Common::fs::path& target_path,
                           std::string& error_message) {
#if PROCESS_INTERFACE_PLATFORM_WINDOWS
    const std::wstring temp_w = temp_path.wstring();
    const std::wstring target_w = target_path.wstring();

    const int move_rc = MoveFileExW(
        temp_w.c_str(),
        target_w.c_str(),
        kMoveFileReplaceExisting | kMoveFileWriteThrough);

    if (move_rc == 0) {
        error_message = "failed to replace file: " + target_path.string();
        return false;
    }
    return true;
#else
    std::error_code rename_ec;
    Common::fs::rename(temp_path, target_path, rename_ec);
    if (rename_ec) {
        error_message = "failed to replace file: " + target_path.string();
        return false;
    }
    return SyncParentDirectory(target_path, error_message);
#endif
}

}  // namespace

bool AtomicReplaceFile(const Common::fs::path& target_path, const std::string& contents, std::string& error_message) {
    std::error_code ec;
    Common::fs::create_directories(target_path.parent_path(), ec);
    if (ec) {
        error_message = "failed to create parent directory: " + target_path.parent_path().string();
        return false;
    }

    const Common::fs::path temp_path = BuildTempPath(target_path);
    if (!WriteTempFileDurable(temp_path, contents, error_message)) {
        return false;
    }

    if (!ReplaceTargetWithTemp(temp_path, target_path, error_message)) {
        std::error_code remove_ec;
        Common::fs::remove(temp_path, remove_ec);
        return false;
    }

    return true;
}

}  // namespace Platform
}  // namespace ProcessInterface
