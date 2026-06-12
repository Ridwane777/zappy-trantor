#include "../include/Server.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdexcept>
#include <iostream>
#include <cstring>

Server::Server(const ServerConfig &config) : _fd(-1), _config(config)
{
    _createSocket();
    _bindAndListen();

    for (const auto &team : _config.teams)
        _slots[team] = _config.clients;
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

bool Server::_handleClient(std::size_t index)
{
    char buf[4096];
    int n = read(_fds[index].fd, buf, sizeof(buf));
    std::size_t clientIndex = index - 1;

    if (n <= 0) {
        std::cout << "Client disconnected: fd=" << _fds[index].fd << std::endl;
        close(_fds[index].fd);
        _fds.erase(_fds.begin() + index);
        _clients.erase(_clients.begin() + clientIndex);
        return true;
    }
    _clients[clientIndex].appendToReadBuffer(buf, n);
    std::cout << "fd=" << _fds[index].fd << " received: "
              << _clients[clientIndex].getReadBuffer();
    return false;
}

void Server::_flushClient(std::size_t index)
{
    std::size_t clientIndex = index - 1;
    Client &client = _clients[clientIndex];
    const std::string &buffer = client.getWriteBuffer();

    int n = write(_fds[index].fd, buffer.data(), buffer.size());
    if (n > 0)
        client.consumeWriteBuffer(n);
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

        if (poll(_fds.data(), _fds.size(), -1) == -1)
            throw std::runtime_error("poll() failed");

        if (_fds[0].revents & POLLIN)
            _acceptClient();

        for (std::size_t i = 1; i < _fds.size(); i++) {
            bool disconnected = false;

            if (_fds[i].revents & POLLIN)
                disconnected = _handleClient(i);
            if (!disconnected && (_fds[i].revents & POLLOUT))
                _flushClient(i);
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
