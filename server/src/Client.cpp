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

const std::string &Client::getWriteBuffer() const
{
    return _writeBuffer;
}

void Client::queueWrite(const std::string &data)
{
    _writeBuffer.append(data);
}

void Client::consumeWriteBuffer(std::size_t len)
{
    _writeBuffer.erase(0, len);
}

bool Client::hasPendingWrite() const
{
    return !_writeBuffer.empty();
}
