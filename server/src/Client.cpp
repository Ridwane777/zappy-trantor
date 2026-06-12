#include "../include/Client.hpp"

Client::Client(int fd) : _fd(fd)
{
}

int Client::getFd() const
{
    return _fd;
}

ClientState Client::getState() const
{
    return _state;
}

void Client::setState(ClientState state)
{
    _state = state;
}

const std::string &Client::getTeam() const
{
    return _team;
}

void Client::setTeam(const std::string &team)
{
    _team = team;
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
