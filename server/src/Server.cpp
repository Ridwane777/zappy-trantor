#include "../include/Server.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdexcept>
#include <iostream>
#include <cstring>
#include <cstdlib>

Server::Server(const ServerConfig &config) : _fd(-1), _config(config), _world(_config)
{
    _createSocket();
    _bindAndListen();

    for (const auto &team : _config.teams)
        _slots[team] = _config.clients;

    _world.onGuiEvent = [this](const std::string &msg) {
        _broadcastToGui(msg);
    };
    _world.onPlayerEvent = [this](int fd, const std::string &msg) {
        _sendToPlayer(fd, msg);
    };
    _lastReset = std::chrono::steady_clock::now();
    if (const char *env = std::getenv("ZAPPY_RESET_INTERVAL")) {
        double v = std::atof(env);
        if (v > 0.0)
            _resetInterval = v;
    }
}

double Server::_secondsToNextReset() const
{
    double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - _lastReset).count();
    double remaining = _resetInterval - elapsed;
    return remaining > 0.0 ? remaining : 0.0;
}

void Server::_broadcastToGui(const std::string &msg)
{
    for (auto &client : _clients)
        if (client.getState() == ClientState::GUI)
            client.queueWrite(msg);
}

void Server::_sendToPlayer(int fd, const std::string &msg)
{
    for (auto &client : _clients)
        if (client.getFd() == fd) {
            client.queueWrite(msg);
            return;
        }
}

void Server::_sendGuiBootstrap(Client &client)
{
    client.queueWrite("msz " + std::to_string(_config.width) + " " +
                       std::to_string(_config.height) + "\n");
    for (const auto &msg : _world.getGuiSnapshot())
        client.queueWrite(msg);
    client.queueWrite("sgt " + std::to_string(_config.freq) + "\n");
    // Custom GUI-only line: seconds until the next resource reset (for countdown).
    client.queueWrite("sst " + std::to_string((int)_secondsToNextReset()) + "\n");
}

void Server::_bindAndListen()
{
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(_config.port);

    if (bind(_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
        throw std::runtime_error("bind() failed");
    if (listen(_fd, SOMAXCONN) == -1)
        throw std::runtime_error("listen() failed");

    std::cout << "Server listening on port " << _config.port << std::endl;
}

void Server::_createSocket()
{
    _fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_fd == -1)
        throw std::runtime_error("socket() failed");

    int opt = 1;
    setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (fcntl(_fd, F_SETFL, O_NONBLOCK) == -1)
        throw std::runtime_error("fcntl() failed");
}

void Server::_acceptClient()
{
    int clientFd = accept(_fd, nullptr, nullptr);
    if (clientFd == -1)
        return;

    if (fcntl(clientFd, F_SETFL, O_NONBLOCK) == -1) {
        close(clientFd);
        return;
    }
    _fds.push_back({clientFd, POLLIN, 0});
    _clients.push_back(Client(clientFd));
    _clients.back().queueWrite("WELCOME\n");
    std::cout << "Client connected: fd=" << clientFd << std::endl;
}

void Server::_processHandshake(std::size_t index, const std::string &line)
{
    Client &client = _clients[index - 1];

    if (line == "GRAPHIC") {
        client.setState(ClientState::GUI);
        _sendGuiBootstrap(client);
        return;
    }

    auto it = _slots.find(line);
    if (it != _slots.end() && it->second > 0) {
        it->second--;
        client.setState(ClientState::AI);
        client.setTeam(line);
        client.queueWrite(std::to_string(it->second) + "\n");
        client.queueWrite(std::to_string(_config.width) + " " +
                           std::to_string(_config.height) + "\n");
        _world.addPlayer(client.getFd(), line);
        return;
    }
    // No initial slot left — check if a Fork egg exists for this team.
    // If so, hatch it: the child process spawned by do_fork() becomes a player.
    if (it != _slots.end() && _world.hatchEgg(line)) {
        client.setState(ClientState::AI);
        client.setTeam(line);
        client.queueWrite("0\n");
        client.queueWrite(std::to_string(_config.width) + " " +
                           std::to_string(_config.height) + "\n");
        _world.addPlayer(client.getFd(), line);
        return;
    }

    client.queueWrite("ko\n");
    client.setState(ClientState::CLOSING);
}

bool Server::_handleClient(std::size_t index)
{
    char buf[4096];
    int n = read(_fds[index].fd, buf, sizeof(buf));
    std::size_t clientIndex = index - 1;

    if (n <= 0) {
        std::cout << "Client disconnected: fd=" << _fds[index].fd << std::endl;
        if (_clients[clientIndex].getState() == ClientState::AI)
            _world.removePlayer(_fds[index].fd);
        close(_fds[index].fd);
        _fds.erase(_fds.begin() + index);
        _clients.erase(_clients.begin() + clientIndex);
        return true;
    }
    Client &client = _clients[clientIndex];
    client.appendToReadBuffer(buf, n);

    // Process every complete line: the first is the team handshake, the rest are
    // gameplay commands from AI clients (GUI clients don't send commands).
    std::string line;
    while (client.extractLine(line)) {
        if (client.getState() == ClientState::WAITING_TEAM_NAME) {
            _processHandshake(index, line);
        } else if (client.getState() == ClientState::AI) {
            std::string resp = _world.executeCommand(client.getFd(), line);
            if (!resp.empty())                 // empty = command sent its own reply
                client.queueWrite(resp + "\n");
        } else if (client.getState() == ClientState::GUI) {
            // GUI requests: "pin #id" → reply with that player's inventory
            std::size_t h = line.find('#');
            if (line.rfind("pin", 0) == 0 && h != std::string::npos) {
                try {
                    std::string resp = _world.guiPlayerInventory(std::stoi(line.substr(h + 1)));
                    if (!resp.empty())
                        client.queueWrite(resp);
                } catch (...) {}
            }
        }
    }

    return false;
}

bool Server::_flushClient(std::size_t index)
{
    std::size_t clientIndex = index - 1;
    Client &client = _clients[clientIndex];
    const std::string &buffer = client.getWriteBuffer();

    int n = write(_fds[index].fd, buffer.data(), buffer.size());
    if (n > 0)
        client.consumeWriteBuffer(n);

    if (client.getState() == ClientState::CLOSING && !client.hasPendingWrite()) {
        close(_fds[index].fd);
        _fds.erase(_fds.begin() + index);
        _clients.erase(_clients.begin() + clientIndex);
        return true;
    }
    return false;
}

void Server::run()
{
    _fds.push_back({_fd, POLLIN, 0});

    while (true) {
        for (std::size_t i = 1; i < _fds.size(); i++) {
            _fds[i].events = POLLIN;
            if (_clients[i - 1].hasPendingWrite())
                _fds[i].events |= POLLOUT;
        }

        // Wake at least every 500 ms so the resource-reset cycle fires on time
        // even when no client is sending anything.
        int timeoutMs = static_cast<int>(_secondsToNextReset() * 1000.0);
        if (timeoutMs < 0 || timeoutMs > 500)
            timeoutMs = 500;
        if (poll(_fds.data(), _fds.size(), timeoutMs) == -1)
            throw std::runtime_error("poll() failed");

        if (_secondsToNextReset() <= 0.0) {            // resource reset due
            _world.respawnAndBroadcast();
            _lastReset = std::chrono::steady_clock::now();
            _broadcastToGui("sst " + std::to_string((int)_resetInterval) + "\n");
        }

        if (_fds[0].revents & POLLIN)
            _acceptClient();

        for (std::size_t i = 1; i < _fds.size(); i++) {
            bool disconnected = false;

            if (_fds[i].revents & POLLIN)
                disconnected = _handleClient(i);
            if (!disconnected && (_fds[i].revents & POLLOUT))
                disconnected = _flushClient(i);
            if (disconnected)
                i--;
        }
    }
}

Server::~Server()
{
    for (auto &pfd : _fds)
        close(pfd.fd);
    if (_fd != -1)
        close(_fd);
}
