#include "Server.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdexcept>
#include <iostream>
#include <cstring>

Server::Server(const ServerConfig &config) : _fd(-1), _config(config)
{
    _createSocket();
    _bindAndListen();
}

void Server::_createSocket()
{
    _fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_fd == -1)
        throw std::runtime_error("socket() failed");

    int opt = 1;
    setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
}

Server::~Server()
{
    for (auto &pfd : _fds)
        close(pfd.fd);
    if (_fd != -1)
        close(_fd);
}
