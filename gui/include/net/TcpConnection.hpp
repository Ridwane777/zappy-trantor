#pragma once

#include <string>
#include <vector>

// One TCP connection, line by line, never blocking the render loop.
// The only GUI code that calls the C socket API. Knows nothing of the protocol.
class TcpConnection {
public:
    TcpConnection() = default;
    ~TcpConnection();
    TcpConnection(const TcpConnection &) = delete;
    TcpConnection &operator=(const TcpConnection &) = delete;

    // Connects within `timeoutMs` (non-blocking connect + poll), stays non-blocking.
    bool open(const std::string &host, int port, int timeoutMs, std::string &err);
    void close();
    bool isOpen() const { return _fd >= 0; }

    // Waits for one line at most `timeoutMs` (used for the handshake).
    bool readLineBlocking(std::string &line, int timeoutMs, std::string &err);

    // Queues a line; it is written when the socket is writable.
    void sendLine(const std::string &line);

    // poll() once without waiting: flushes pending output, reads available input.
    // Returns false when the connection is lost (the reason is in lastError()).
    bool pump();

    // Complete lines received so far, oldest first, at most `max`.
    std::vector<std::string> takeLines(std::size_t max);

    const std::string &lastError() const { return _error; }

private:
    int _fd = -1;
    std::string _in;
    std::string _out;
    std::string _error;

    bool readAvailable();
    bool flush();
    bool fail(const std::string &what);
};
