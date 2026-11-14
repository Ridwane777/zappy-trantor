#include "net/TcpConnection.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

// Frees getaddrinfo()'s result on every path.
struct AddrInfoDeleter {
    void operator()(addrinfo *p) const { freeaddrinfo(p); }
};
using AddrInfoPtr = std::unique_ptr<addrinfo, AddrInfoDeleter>;

constexpr std::size_t MAX_PENDING_INPUT = 1 << 20;

} // namespace

TcpConnection::~TcpConnection()
{
    close();
}

void TcpConnection::close()
{
    if (_fd >= 0)
        ::close(_fd);
    _fd = -1;
    _in.clear();
    _out.clear();
}

bool TcpConnection::fail(const std::string &what)
{
    _error = what + (errno ? std::string(": ") + std::strerror(errno) : "");
    close();
    return false;
}

bool TcpConnection::open(const std::string &host, int port, int timeoutMs, std::string &err)
{
    close();
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo *raw = nullptr;
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &raw) != 0 || !raw) {
        err = "cannot resolve host '" + host + "'";
        return false;
    }
    AddrInfoPtr res(raw);

    _fd = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (_fd < 0 || fcntl(_fd, F_SETFL, fcntl(_fd, F_GETFL, 0) | O_NONBLOCK) == -1) {
        fail("socket");
        err = _error;
        return false;
    }
    if (::connect(_fd, res->ai_addr, res->ai_addrlen) < 0 && errno != EINPROGRESS) {
        fail("connect");
        err = _error;
        return false;
    }
    pollfd p{_fd, POLLOUT, 0};
    int soErr = 0;
    socklen_t len = sizeof(soErr);
    if (::poll(&p, 1, timeoutMs) <= 0 || getsockopt(_fd, SOL_SOCKET, SO_ERROR, &soErr, &len) < 0 || soErr) {
        errno = soErr ? soErr : ETIMEDOUT;
        fail("connect");
        err = _error;
        return false;
    }
    return true;
}

bool TcpConnection::readLineBlocking(std::string &line, int timeoutMs, std::string &err)
{
    while (_in.find('\n') == std::string::npos) {
        pollfd p{_fd, POLLIN, 0};
        if (_fd < 0 || ::poll(&p, 1, timeoutMs) <= 0 || !readAvailable()) {
            err = _error.empty() ? "no answer from server (timeout)" : _error;
            return false;
        }
    }
    line = takeLines(1).front();
    return true;
}

void TcpConnection::sendLine(const std::string &line)
{
    if (_fd >= 0)
        _out += line + "\n";
}

bool TcpConnection::pump()
{
    if (_fd < 0)
        return false;
    pollfd p{_fd, static_cast<short>(POLLIN | (_out.empty() ? 0 : POLLOUT)), 0};
    if (::poll(&p, 1, 0) < 0 && errno != EINTR)
        return fail("poll");
    if ((p.revents & POLLOUT) && !flush())
        return false;
    if ((p.revents & (POLLIN | POLLHUP | POLLERR)) && !readAvailable())
        return false;
    return true;
}

bool TcpConnection::readAvailable()
{
    char buf[8192];
    while (true) {
        ssize_t n = ::read(_fd, buf, sizeof(buf));
        if (n > 0) {
            if (_in.size() + static_cast<std::size_t>(n) > MAX_PENDING_INPUT) {
                errno = 0;
                return fail("server sends faster than the GUI can display");
            }
            _in.append(buf, static_cast<std::size_t>(n));
            continue;
        }
        if (n == 0) {
            errno = 0;
            return fail("server closed the connection");
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return true;
        if (errno != EINTR)
            return fail("read");
    }
}

bool TcpConnection::flush()
{
    while (!_out.empty()) {
        ssize_t n = ::send(_fd, _out.data(), _out.size(), MSG_NOSIGNAL);
        if (n > 0) {
            _out.erase(0, static_cast<std::size_t>(n));
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return true;
        if (n < 0 && errno == EINTR)
            continue;
        return fail("write");
    }
    return true;
}

std::vector<std::string> TcpConnection::takeLines(std::size_t max)
{
    std::vector<std::string> lines;
    std::size_t start = 0;
    std::size_t nl;
    while (lines.size() < max && (nl = _in.find('\n', start)) != std::string::npos) {
        std::string line = _in.substr(start, nl - start);
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        lines.push_back(std::move(line));
        start = nl + 1;
    }
    _in.erase(0, start);
    return lines;
}
