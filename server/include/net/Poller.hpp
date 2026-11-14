#pragma once

#include <vector>

namespace zappy::net {

// Thin wrapper over poll(): the caller lists what to watch, gets back what is ready.
class Poller {
public:
    struct Watch {
        int fd;
        bool read;
        bool write;
    };
    struct Ready {
        int fd;
        bool readable;
        bool writable;
        bool failed;   // POLLERR / POLLHUP / POLLNVAL
    };

    // Waits at most `timeoutMs` ms. A signal interruption returns no event.
    std::vector<Ready> wait(const std::vector<Watch> &watches, int timeoutMs);
};

} // namespace zappy::net
