#include "net/Poller.hpp"
#include "net/Socket.hpp"
#include <cerrno>
#include <poll.h>

namespace zappy::net {

std::vector<Poller::Ready> Poller::wait(const std::vector<Watch> &watches, int timeoutMs)
{
    std::vector<pollfd> fds;
    fds.reserve(watches.size());
    for (const auto &w : watches)
        fds.push_back({w.fd, static_cast<short>((w.read ? POLLIN : 0) | (w.write ? POLLOUT : 0)), 0});

    int n = ::poll(fds.data(), fds.size(), timeoutMs);
    if (n == -1) {
        if (errno == EINTR)
            return {};
        throw SystemError("poll", errno);
    }

    std::vector<Ready> ready;
    for (const auto &p : fds) {
        if (p.revents == 0)
            continue;
        ready.push_back({p.fd, (p.revents & POLLIN) != 0, (p.revents & POLLOUT) != 0,
                         (p.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0});
    }
    return ready;
}

} // namespace zappy::net
