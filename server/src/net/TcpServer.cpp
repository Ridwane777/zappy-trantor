#include "net/TcpServer.hpp"
#include <cstring>
#include <iostream>
#include <vector>

namespace zappy::net {

TcpServer::TcpServer(int port, TcpServerLimits limits)
    : _limits(limits), _listener(Socket::listenTcp(port))
{
}

void TcpServer::pollOnce(int timeoutMs)
{
    std::vector<Poller::Watch> watches{{_listener.fd(), true, false}};
    std::map<int, ConnectionId> byFd;
    for (auto &[id, c] : _connections) {
        watches.push_back({c.socket.fd(), !c.closing, !c.out.empty()});
        byFd[c.socket.fd()] = id;
    }

    for (const auto &ready : _poller.wait(watches, timeoutMs)) {
        if (ready.fd == _listener.fd()) {
            acceptAll();
            continue;
        }
        auto id = byFd.find(ready.fd);
        if (id == byFd.end())
            continue;
        auto it = _connections.find(id->second);
        if (it == _connections.end() || it->second.dropped)
            continue;
        if (ready.readable || ready.failed)
            readFrom(it->first, it->second);
        if (!it->second.dropped && ready.writable)
            flush(it->first, it->second);
    }
    removeClosed();
}

void TcpServer::acceptAll()
{
    while (auto socket = _listener.accept()) {
        ConnectionId id = _nextId++;
        _connections.emplace(id, Connection{std::move(*socket), RingBuffer(_limits.inputBuffer),
                                            RingBuffer(_limits.outputBuffer)});
        if (_handler)
            _handler->onConnect(id);
    }
}

void TcpServer::readFrom(ConnectionId id, Connection &c)
{
    if (c.in.full()) {
        drop(id, c, "line longer than " + std::to_string(_limits.inputBuffer) + " bytes");
        return;
    }
    auto result = c.socket.readSome(c.in.writePtr(), c.in.contiguousWritable());
    switch (result.status) {
    case Socket::Status::WouldBlock:
        return;
    case Socket::Status::Closed:
        drop(id, c, "disconnected");
        return;
    case Socket::Status::Error:
        drop(id, c, std::string("read error: ") + std::strerror(result.error));
        return;
    case Socket::Status::Ok:
        c.in.commit(result.bytes);
        break;
    }
    std::string line;
    while (!c.dropped && !c.closing && c.in.extractLine(line))
        if (_handler)
            _handler->onLine(id, line);
}

void TcpServer::flush(ConnectionId id, Connection &c)
{
    while (!c.out.empty()) {
        auto result = c.socket.writeSome(c.out.readPtr(), c.out.contiguousReadable());
        if (result.status == Socket::Status::WouldBlock)
            return;
        if (result.status == Socket::Status::Error) {
            drop(id, c, std::string("write error: ") + std::strerror(result.error));
            return;
        }
        c.out.consume(result.bytes);
    }
}

void TcpServer::send(ConnectionId id, const std::string &text)
{
    auto it = _connections.find(id);
    if (it == _connections.end() || it->second.dropped)
        return;
    if (!it->second.out.writeAll(text))
        drop(id, it->second, "output buffer full (client does not read its replies)");
}

void TcpServer::closeAfterFlush(ConnectionId id)
{
    auto it = _connections.find(id);
    if (it != _connections.end())
        it->second.closing = true;
}

void TcpServer::drop(ConnectionId id, Connection &c, const std::string &reason)
{
    if (c.dropped)
        return;
    c.dropped = true;
    std::cerr << "[net] connection " << id << " closed: " << reason << "\n";
}

void TcpServer::removeClosed()
{
    for (auto it = _connections.begin(); it != _connections.end();) {
        Connection &c = it->second;
        if (c.dropped || (c.closing && c.out.empty())) {
            ConnectionId id = it->first;
            it = _connections.erase(it);
            if (_handler)
                _handler->onDisconnect(id);
        } else {
            ++it;
        }
    }
}

} // namespace zappy::net
