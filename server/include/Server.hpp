#pragma once

#include "Args.hpp"
#include "Client.hpp"
#include <vector>
#include <poll.h>

class Server {
public:
    Server(const ServerConfig &config);
    ~Server();

    void run();

private:
    int _fd;
    ServerConfig _config;
    std::vector<struct pollfd> _fds;
    std::vector<Client> _clients;

    void _createSocket();
    void _bindAndListen();
    void _acceptClient();
    void _handleClient(std::size_t index);
    void _flushClient(std::size_t index);
};
