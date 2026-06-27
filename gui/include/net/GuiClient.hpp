#pragma once
// TCP client that speaks the Zappy GRAPHIC protocol to the server.
//
// Usage:
//   GuiClient c;
//   if (!c.connect("127.0.0.1", 4242, err)) { ...show err... }
//   // each frame:
//   c.poll();                 // non-blocking: drains socket, updates state()
//   const GuiState& s = c.state();
//
// connect() blocks briefly to establish the TCP session and perform the
// WELCOME/GRAPHIC handshake, then switches the socket to non-blocking so poll()
// never stalls the render loop. Partial lines are buffered between polls.

#include "net/GuiState.hpp"
#include <string>
#include <deque>

class GuiClient {
public:
    GuiClient() = default;
    ~GuiClient();

    GuiClient(const GuiClient &) = delete;
    GuiClient &operator=(const GuiClient &) = delete;

    // Establish the connection and send "GRAPHIC". On failure returns false and
    // fills `err`. On success the socket is left non-blocking.
    bool connect(const std::string &host, int port, std::string &err);

    // Drain whatever the server has sent and apply it to the state.
    // Sets connected() to false if the peer closes or errors.
    void poll();

    void disconnect();

    // Send a request line to the server (e.g. "pin #3"). A newline is appended.
    // No-op if not connected. Best-effort (non-blocking socket).
    void send(const std::string &line);

    bool connected() const { return _fd >= 0; }
    const GuiState &state() const { return _state; }
    GuiState &state() { return _state; }

    // Transient incantation flashes since last drain (renderer consumes these).
    std::deque<GuiIncantation> &incantationEvents() { return _incantations; }

private:
    int       _fd = -1;
    GuiState  _state;
    std::string _readBuf;                 // accumulates partial lines
    std::deque<GuiIncantation> _incantations;

    void _processLine(const std::string &line);
    // Append an action-feed entry (looks up the player's team for coloring).
    void _logEvent(int id, const std::string &action, int kind);
};
