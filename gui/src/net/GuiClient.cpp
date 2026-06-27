#include "net/GuiClient.hpp"

#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <sstream>

// Parse a player/egg id token, tolerating an optional leading '#'.
static int parseId(const std::string &tok)
{
    if (!tok.empty() && tok[0] == '#')
        return std::atoi(tok.c_str() + 1);
    return std::atoi(tok.c_str());
}

static const char *resName(int i)
{
    static const char *N[] = {"food", "linemate", "deraumere", "sibur",
                              "mendiane", "phiras", "thystame"};
    return (i >= 0 && i < 7) ? N[i] : "?";
}

void GuiClient::_logEvent(int id, const std::string &action, int kind)
{
    std::string team;
    auto it = _state.players.find(id);
    if (it != _state.players.end())
        team = it->second.team;
    _state.log.push_back({id, team, action, kind});
    _state.logTotal++;
    if (_state.log.size() > 100)
        _state.log.pop_front();
}

GuiClient::~GuiClient()
{
    disconnect();
}

void GuiClient::disconnect()
{
    if (_fd >= 0) {
        close(_fd);
        _fd = -1;
    }
}

void GuiClient::send(const std::string &line)
{
    if (_fd < 0)
        return;
    std::string out = line;
    out.push_back('\n');
    ssize_t n = ::write(_fd, out.data(), out.size());
    (void)n;   // best-effort; non-blocking, GUI requests are idempotent
}

bool GuiClient::connect(const std::string &host, int port, std::string &err)
{
    struct addrinfo hints{};
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *res = nullptr;
    std::string portStr = std::to_string(port);
    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0 || !res) {
        err = "cannot resolve host '" + host + "'";
        return false;
    }

    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) {
        err = std::string("socket() failed: ") + std::strerror(errno);
        freeaddrinfo(res);
        return false;
    }

    // bound handshake timeout so a wrong host/port can't hang the menu
    struct timeval tv{};
    tv.tv_sec = 5;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    if (::connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
        err = std::string("connect() failed: ") + std::strerror(errno);
        close(fd);
        freeaddrinfo(res);
        return false;
    }
    freeaddrinfo(res);

    std::string welcome;
    char ch;
    while (welcome.size() < 64) {
        ssize_t n = read(fd, &ch, 1);
        if (n <= 0) {
            err = "no WELCOME from server (timeout or closed)";
            close(fd);
            return false;
        }
        if (ch == '\n')
            break;
        welcome += ch;
    }
    if (welcome.rfind("WELCOME", 0) != 0) {
        err = "unexpected greeting: '" + welcome + "'";
        close(fd);
        return false;
    }

    const char *graphic = "GRAPHIC\n";
    if (write(fd, graphic, std::strlen(graphic)) < 0) {
        err = std::string("write() failed: ") + std::strerror(errno);
        close(fd);
        return false;
    }

    fcntl(fd, F_SETFL, O_NONBLOCK);
    _fd = fd;
    _readBuf.clear();
    _state = GuiState{};
    return true;
}
