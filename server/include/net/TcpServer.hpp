#pragma once

#include "net/IConnectionHandler.hpp"
#include "net/Poller.hpp"
#include "net/RingBuffer.hpp"
#include "net/Socket.hpp"
#include <cstddef>
#include <map>
#include <string>

namespace zappy::net {

struct TcpServerLimits {
    std::size_t inputBuffer = 4096;          // longest accepted line
    std::size_t outputBuffer = 1024 * 1024;  // unsent data allowed per client
};

// Accepts clients, reads their lines and writes their replies, with poll().
// It forwards everything to an IConnectionHandler and knows nothing else.
class TcpServer : public IClientSink {
public:
    TcpServer(int port, TcpServerLimits limits = {});

    void setHandler(IConnectionHandler &handler) { _handler = &handler; }

    // Waits up to `timeoutMs` for network activity and handles it.
    void pollOnce(int timeoutMs);

    void send(ConnectionId id, const std::string &text) override;
    void closeAfterFlush(ConnectionId id) override;

    std::size_t connectionCount() const { return _connections.size(); }

private:
    struct Connection {
        Socket socket;
        RingBuffer in;
        RingBuffer out;
        bool closing = false;   // close once `out` is empty
        bool dropped = false;   // close now, discard pending output
    };

    TcpServerLimits _limits;
    Socket _listener;
    Poller _poller;
    IConnectionHandler *_handler = nullptr;
    std::map<ConnectionId, Connection> _connections;
    ConnectionId _nextId = 1;

    void acceptAll();
    void readFrom(ConnectionId id, Connection &c);
    void flush(ConnectionId id, Connection &c);
    void drop(ConnectionId id, Connection &c, const std::string &reason);
    void removeClosed();
};

} // namespace zappy::net
