#pragma once

#include <string>

class Client {
public:
    Client(int fd);

    int getFd() const;

    const std::string &getReadBuffer() const;
    void appendToReadBuffer(const char *data, std::size_t len);
    void clearReadBuffer();

    const std::string &getWriteBuffer() const;
    void queueWrite(const std::string &data);
    void consumeWriteBuffer(std::size_t len);
    bool hasPendingWrite() const;

private:
    int _fd;
    std::string _readBuffer;
    std::string _writeBuffer;
};
