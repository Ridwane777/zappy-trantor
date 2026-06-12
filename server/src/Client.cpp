#include "../include/Client.hpp"

Client::Client(int fd) : _fd(fd)
{
}

int Client::getFd() const
{
    return _fd;
}

const std::string &Client::getReadBuffer() const
{
    return _readBuffer;
}

void Client::appendToReadBuffer(const char *data, std::size_t len)
{
    _readBuffer.append(data, len);
}

void Client::clearReadBuffer()
{
    _readBuffer.clear();
}
