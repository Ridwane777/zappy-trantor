#pragma once

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>

namespace zappy::net {

// Thrown when a system call fails; carries the call name and errno text.
class SystemError : public std::runtime_error {
public:
    SystemError(const std::string &call, int err);
};

// Owns one file descriptor and closes it exactly once. Move-only.
// This is the only place of the server that calls the C socket API.
class Socket {
public:
    Socket() = default;
    explicit Socket(int fd) : _fd(fd) {}
    ~Socket();
    Socket(const Socket &) = delete;
    Socket &operator=(const Socket &) = delete;
    Socket(Socket &&other) noexcept;
    Socket &operator=(Socket &&other) noexcept;

    int fd() const { return _fd; }
    bool valid() const { return _fd >= 0; }

    // Non-blocking TCP socket listening on all interfaces.
    static Socket listenTcp(int port);
    // Next pending connection (non-blocking), or nothing when none is waiting.
    std::optional<Socket> accept() const;

    enum class Status { Ok, WouldBlock, Closed, Error };
    struct IoResult {
        Status status;
        std::size_t bytes;
        int error;   // errno when status == Error
    };
    IoResult readSome(char *dst, std::size_t len) const;
    IoResult writeSome(const char *src, std::size_t len) const;

    void setNonBlocking() const;

private:
    int _fd = -1;
    void close();
};

} // namespace zappy::net
