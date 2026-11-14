#pragma once

#include <string>

namespace zappy::net {

using ConnectionId = int;

// What the application can ask the network layer to do.
class IClientSink {
public:
    virtual ~IClientSink() = default;
    // Queues `text` for the client. A client whose output buffer overflows is dropped.
    virtual void send(ConnectionId id, const std::string &text) = 0;
    // Closes the connection once everything queued has been sent.
    virtual void closeAfterFlush(ConnectionId id) = 0;
};

// What the network layer tells the application. The network layer only knows
// this interface: it never includes game or protocol code.
class IConnectionHandler {
public:
    virtual ~IConnectionHandler() = default;
    virtual void onConnect(ConnectionId id) = 0;
    // One complete line received (without the trailing newline).
    virtual void onLine(ConnectionId id, const std::string &line) = 0;
    virtual void onDisconnect(ConnectionId id) = 0;
};

} // namespace zappy::net
