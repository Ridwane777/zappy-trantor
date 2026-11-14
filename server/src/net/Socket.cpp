#include "net/Socket.hpp"
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace zappy::net {

SystemError::SystemError(const std::string &call, int err)
    : std::runtime_error(call + "() failed: " + std::strerror(err))
{
}

Socket::~Socket()
{
    close();
}

Socket::Socket(Socket &&other) noexcept : _fd(other._fd)
{
    other._fd = -1;
}

Socket &Socket::operator=(Socket &&other) noexcept
{
    if (this != &other) {
        close();
        _fd = other._fd;
        other._fd = -1;
    }
    return *this;
}

void Socket::close()
{
    if (_fd >= 0)
        ::close(_fd);
    _fd = -1;
}

void Socket::setNonBlocking() const
{
    int flags = ::fcntl(_fd, F_GETFL, 0);
    if (flags == -1 || ::fcntl(_fd, F_SETFL, flags | O_NONBLOCK) == -1)
        throw SystemError("fcntl", errno);
}

Socket Socket::listenTcp(int port)
{
    Socket s(::socket(AF_INET, SOCK_STREAM, 0));
    if (!s.valid())
        throw SystemError("socket", errno);
    int on = 1;
    if (::setsockopt(s._fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1)
        throw SystemError("setsockopt", errno);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (::bind(s._fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == -1)
        throw SystemError("bind", errno);
    if (::listen(s._fd, SOMAXCONN) == -1)
        throw SystemError("listen", errno);
    s.setNonBlocking();
    return s;
}

std::optional<Socket> Socket::accept() const
{
    int fd = ::accept(_fd, nullptr, nullptr);
    if (fd == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR || errno == ECONNABORTED)
            return std::nullopt;
        throw SystemError("accept", errno);
    }
    Socket client(fd);
    client.setNonBlocking();
    return client;
}

Socket::IoResult Socket::readSome(char *dst, std::size_t len) const
{
    ssize_t n = ::read(_fd, dst, len);
    if (n > 0)
        return {Status::Ok, static_cast<std::size_t>(n), 0};
    if (n == 0)
        return {Status::Closed, 0, 0};
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
        return {Status::WouldBlock, 0, 0};
    return {Status::Error, 0, errno};
}

Socket::IoResult Socket::writeSome(const char *src, std::size_t len) const
{
    // MSG_NOSIGNAL: a peer that closed returns EPIPE instead of killing the server with SIGPIPE.
    ssize_t n = ::send(_fd, src, len, MSG_NOSIGNAL);
    if (n >= 0)
        return {Status::Ok, static_cast<std::size_t>(n), 0};
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
        return {Status::WouldBlock, 0, 0};
    return {Status::Error, 0, errno};
}

} // namespace zappy::net
