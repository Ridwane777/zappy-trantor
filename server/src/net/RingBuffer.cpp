#include "net/RingBuffer.hpp"
#include <algorithm>
#include <stdexcept>

namespace zappy::net {

RingBuffer::RingBuffer(std::size_t capacity) : _data(capacity)
{
    if (capacity == 0)
        throw std::invalid_argument("RingBuffer capacity must be > 0");
}

std::size_t RingBuffer::write(const char *src, std::size_t len)
{
    std::size_t stored = 0;
    while (stored < len && !full()) {
        std::size_t chunk = std::min(len - stored, contiguousWritable());
        std::copy_n(src + stored, chunk, writePtr());
        commit(chunk);
        stored += chunk;
    }
    return stored;
}

bool RingBuffer::writeAll(const std::string &text)
{
    if (text.size() > freeSpace())
        return false;
    write(text.data(), text.size());
    return true;
}

bool RingBuffer::extractLine(std::string &line)
{
    for (std::size_t i = 0; i < _size; ++i) {
        if (_data[(_head + i) % _data.size()] != '\n')
            continue;
        line.clear();
        line.reserve(i);
        for (std::size_t j = 0; j < i; ++j)
            line.push_back(_data[(_head + j) % _data.size()]);
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        consume(i + 1);
        return true;
    }
    return false;
}

std::size_t RingBuffer::contiguousReadable() const
{
    return std::min(_size, _data.size() - _head);
}

void RingBuffer::consume(std::size_t len)
{
    len = std::min(len, _size);
    _head = (_head + len) % _data.size();
    _size -= len;
    if (_size == 0)
        _head = 0;
}

std::size_t RingBuffer::contiguousWritable() const
{
    if (full())
        return 0;
    std::size_t tail = tailIndex();
    return tail >= _head ? _data.size() - tail : _head - tail;
}

void RingBuffer::commit(std::size_t len)
{
    _size += std::min(len, freeSpace());
}

} // namespace zappy::net
