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

Server::~Server()
{
    for (auto &pfd : _fds)
        close(pfd.fd);
    if (_fd != -1)
        close(_fd);
}
