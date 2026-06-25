#include "net/GuiClient.hpp"

#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <sstream>

// ── helpers ────────────────────────────────────────────────────────────────────

// Parse a player/egg id token, tolerating an optional leading '#'.
static int parseId(const std::string &tok)
{
    if (!tok.empty() && tok[0] == '#')
        return std::atoi(tok.c_str() + 1);
    return std::atoi(tok.c_str());
}

// Resource index → name (matches the server: 0=food, 1=linemate, ...).
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

// ── lifecycle ──────────────────────────────────────────────────────────────────

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
    (void)n;   // best-effort; non-blocking socket, GUI requests are idempotent
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

    // Bound the handshake so a wrong host/port can't hang the menu.
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

    // Server greets with "WELCOME\n"; reply with "GRAPHIC\n".
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

    // Stream is now non-blocking for the render loop.
    fcntl(fd, F_SETFL, O_NONBLOCK);
    _fd = fd;
    _readBuf.clear();
    _state = GuiState{};
    return true;
}

// ── receive loop ───────────────────────────────────────────────────────────────

void GuiClient::poll()
{
    if (_fd < 0)
        return;

    char buf[8192];
    while (true) {
        ssize_t n = read(_fd, buf, sizeof(buf));
        if (n > 0) {
            _readBuf.append(buf, static_cast<std::size_t>(n));
            continue;
        }
        if (n == 0) {            // peer closed
            disconnect();
            return;
        }
        // n < 0
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            break;               // nothing more for now
        if (errno == EINTR)
            continue;
        disconnect();            // real error
        return;
    }

    std::size_t nl;
    while ((nl = _readBuf.find('\n')) != std::string::npos) {
        std::string line = _readBuf.substr(0, nl);
        _readBuf.erase(0, nl + 1);
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (!line.empty())
            _processLine(line);
    }
}

// ── protocol dispatch ───────────────────────────────────────────────────────────

void GuiClient::_processLine(const std::string &line)
{
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    if (cmd == "msz") {
        int w, h;
        if (iss >> w >> h)
            _state.resizeGrid(w, h);

    } else if (cmd == "sgt") {
        int t;
        if (iss >> t)
            _state.timeUnit = t;

    } else if (cmd == "sst") {
        // custom: seconds until next resource reset (resyncs the GUI countdown)
        int s;
        if (iss >> s)
            _state.secondsToReset = static_cast<float>(s);

    } else if (cmd == "tna") {
        std::string team;
        if (iss >> team)
            _state.teams.push_back(team);

    } else if (cmd == "bct") {
        int x, y;
        if (iss >> x >> y && _state.inBounds(x, y)) {
            GuiTile &t = _state.tileAt(x, y);
            for (int i = 0; i < GUI_RES_COUNT; ++i)
                iss >> t.res[i];
        }

    } else if (cmd == "pnw") {
        GuiPlayer p;
        std::string idTok;
        iss >> idTok >> p.x >> p.y >> p.orientation >> p.level >> p.team;
        p.id = parseId(idTok);
        _state.players[p.id] = p;

    } else if (cmd == "ppo") {
        std::string idTok;
        int x, y, o;
        if (iss >> idTok >> x >> y >> o) {
            auto it = _state.players.find(parseId(idTok));
            if (it != _state.players.end()) {
                it->second.x = x;
                it->second.y = y;
                it->second.orientation = o;
            }
        }

    } else if (cmd == "plv") {
        std::string idTok;
        int lvl;
        if (iss >> idTok >> lvl) {
            int id = parseId(idTok);
            auto it = _state.players.find(id);
            if (it != _state.players.end())
                it->second.level = lvl;
            _logEvent(id, "reached level " + std::to_string(lvl), 1);
        }

    } else if (cmd == "pgt") {
        // player took a resource off a tile
        std::string idTok;
