#include "port_probe.h"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <mutex>
#include <string>

#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
#define PROCESS_INTERFACE_PLATFORM_WINDOWS 1
#else
#define PROCESS_INTERFACE_PLATFORM_WINDOWS 0
#endif

#if PROCESS_INTERFACE_PLATFORM_WINDOWS
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <fcntl.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace ProcessInterface
{
namespace Platform
{

namespace
{

static int ClampTimeoutMs(const int timeout_ms)
{
    int value = timeout_ms;

    if (value <= 0)
    {
        value = 250;
    }

    if (value > 30000)
    {
        value = 30000;
    }

    return value;
}

static bool IsValidPort(const int port)
{
    bool ok = false;

    if (port > 0 && port <= 65535)
    {
        ok = true;
    }

    return ok;
}

#if PROCESS_INTERFACE_PLATFORM_WINDOWS

static bool EnsureWinSockInitialized()
{
    static std::once_flag init_once;
    static bool init_ok = false;

    std::call_once(init_once, []() {
        WSADATA wsa_data;
        init_ok = (::WSAStartup(MAKEWORD(2, 2), &wsa_data) == 0);
        // Intentionally avoid WSACleanup at process shutdown to prevent teardown-order hazards.
    });

    return init_ok;
}

static bool SetNonBlocking(SOCKET s, const bool enable)
{
    bool ok = false;

    u_long mode = enable ? 1UL : 0UL;
    const int rc = ::ioctlsocket(s, FIONBIO, &mode);

    if (rc == 0)
    {
        ok = true;
    }

    return ok;
}

static void CloseSocket(SOCKET s)
{
    if (s != INVALID_SOCKET)
    {
        (void)::closesocket(s);
    }
}

#else

static bool SetNonBlocking(const int fd, const bool enable)
{
    bool ok = false;

    const int flags = ::fcntl(fd, F_GETFL, 0);

    if (flags >= 0)
    {
        int new_flags = flags;

        if (enable)
        {
            new_flags = flags | O_NONBLOCK;
        }
        else
        {
            new_flags = flags & ~O_NONBLOCK;
        }

        const int rc = ::fcntl(fd, F_SETFL, new_flags);

        if (rc == 0)
        {
            ok = true;
        }
    }

    return ok;
}

static void CloseSocket(const int fd)
{
    if (fd >= 0)
    {
        (void)::close(fd);
    }
}

#endif

static bool ConnectWithTimeout(const char* host,
                               const char* port_str,
                               const int timeout_ms)
{
    bool connected = false;

    if (host == NULL || port_str == NULL)
    {
        connected = false;
    }
    else
    {
#if PROCESS_INTERFACE_PLATFORM_WINDOWS
        if (!EnsureWinSockInitialized())
        {
            connected = false;
        }
        else
#endif
        {
            struct addrinfo hints;
            std::memset(&hints, 0, sizeof(hints));

            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_protocol = IPPROTO_TCP;

            struct addrinfo* results = NULL;
            const int gai_rc = ::getaddrinfo(host, port_str, &hints, &results);

            if (gai_rc != 0 || results == NULL)
            {
                connected = false;
            }
            else
            {
                for (struct addrinfo* ai = results; ai != NULL; ai = ai->ai_next)
                {
#if PROCESS_INTERFACE_PLATFORM_WINDOWS
                    SOCKET s = ::socket(ai->ai_family,
                                        ai->ai_socktype,
                                        ai->ai_protocol);
                    if (s == INVALID_SOCKET)
                    {
                        continue;
                    }

                    if (!SetNonBlocking(s, true))
                    {
                        CloseSocket(s);
                        continue;
                    }

                    int rc = ::connect(s, ai->ai_addr,
                                      static_cast<int>(ai->ai_addrlen));

                    if (rc == 0)
                    {
                        connected = true;
                        CloseSocket(s);
                        break;
                    }

                    const int wsa_err = ::WSAGetLastError();
                    if (wsa_err != WSAEWOULDBLOCK
                        && wsa_err != WSAEINPROGRESS
                        && wsa_err != WSAEALREADY
                        && wsa_err != WSAEINVAL)
                    {
                        CloseSocket(s);
                        continue;
                    }

                    fd_set write_fds;
                    FD_ZERO(&write_fds);
                    FD_SET(s, &write_fds);

                    timeval tv;
                    tv.tv_sec = timeout_ms / 1000;
                    tv.tv_usec = (timeout_ms % 1000) * 1000;

                    rc = ::select(0, NULL, &write_fds, NULL, &tv);
                    if (rc > 0 && FD_ISSET(s, &write_fds))
                    {
                        int so_error = 0;
                        int so_len = static_cast<int>(sizeof(so_error));
                        const int grc = ::getsockopt(s,
                                                     SOL_SOCKET,
                                                     SO_ERROR,
                                                     reinterpret_cast<char*>(
                                                         &so_error),
                                                     &so_len);

                        if (grc == 0 && so_error == 0)
                        {
                            connected = true;
                            CloseSocket(s);
                            break;
                        }
                    }

                    CloseSocket(s);
#else
                    int fd = ::socket(ai->ai_family,
                                      ai->ai_socktype,
                                      ai->ai_protocol);
                    if (fd < 0)
                    {
                        continue;
                    }

                    if (!SetNonBlocking(fd, true))
                    {
                        CloseSocket(fd);
                        continue;
                    }

                    int rc = ::connect(fd, ai->ai_addr, ai->ai_addrlen);

                    if (rc == 0)
                    {
                        connected = true;
                        CloseSocket(fd);
                        break;
                    }

                    if (errno != EINPROGRESS)
                    {
                        CloseSocket(fd);
                        continue;
                    }

                    const std::chrono::steady_clock::time_point deadline =
                        std::chrono::steady_clock::now()
                        + std::chrono::milliseconds(timeout_ms);

                    fd_set write_fds;
                    rc = -1;
                    while (true)
                    {
                        const std::chrono::steady_clock::time_point now =
                            std::chrono::steady_clock::now();
                        if (now >= deadline)
                        {
                            rc = 0;
                            break;
                        }

                        const std::chrono::microseconds remaining =
                            std::chrono::duration_cast<std::chrono::microseconds>(
                                deadline - now);

                        FD_ZERO(&write_fds);
                        FD_SET(fd, &write_fds);

                        timeval tv;
                        tv.tv_sec = static_cast<long>(
                            remaining.count() / 1000000LL);
                        tv.tv_usec = static_cast<long>(
                            remaining.count() % 1000000LL);

                        rc = ::select(fd + 1, NULL, &write_fds, NULL, &tv);
                        if (rc < 0 && errno == EINTR)
                        {
                            continue;
                        }
                        if (rc > 0 && !FD_ISSET(fd, &write_fds))
                        {
                            rc = 0;
                        }
                        break;
                    }

                    if (rc > 0 && FD_ISSET(fd, &write_fds))
                    {
                        int so_error = 0;
                        socklen_t so_len = static_cast<socklen_t>(
                            sizeof(so_error));

                        const int grc = ::getsockopt(fd,
                                                     SOL_SOCKET,
                                                     SO_ERROR,
                                                     &so_error,
                                                     &so_len);

                        if (grc == 0 && so_error == 0)
                        {
                            connected = true;
                            CloseSocket(fd);
                            break;
                        }
                    }

                    CloseSocket(fd);
#endif
                }

                ::freeaddrinfo(results);
            }
        }
    }

    return connected;
}

}  // namespace

bool CheckPortListening(const std::string& host, int port, int timeout_ms)
{
    bool ok = false;

    if (host.empty())
    {
        ok = false;
    }
    else if (!IsValidPort(port))
    {
        ok = false;
    }
    else
    {
        const int timeout_value = ClampTimeoutMs(timeout_ms);
        const std::string port_str = std::to_string(port);

        ok = ConnectWithTimeout(host.c_str(), port_str.c_str(), timeout_value);
    }

    return ok;
}

}  // namespace Platform
}  // namespace ProcessInterface
