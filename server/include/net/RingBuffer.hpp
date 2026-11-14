#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace zappy::net {

// Fixed-capacity circular byte buffer. It never grows: a peer that sends or
// ignores more data than the capacity is the caller's problem to handle
// (typically by closing the connection), not the server's memory.
class RingBuffer {
public:
    explicit RingBuffer(std::size_t capacity);

    std::size_t size() const { return _size; }
    std::size_t capacity() const { return _data.size(); }
    std::size_t freeSpace() const { return _data.size() - _size; }
    bool empty() const { return _size == 0; }
    bool full() const { return _size == _data.size(); }

    // Appends up to `len` bytes; returns how many were stored.
    std::size_t write(const char *src, std::size_t len);
    // True when all of `text` fits, and then appends it; false (and no change) otherwise.
    bool writeAll(const std::string &text);

    // Removes the first line (without its '\n', and without a trailing '\r').
    bool extractLine(std::string &line);

    // Contiguous readable bytes from the head (for a single write() call).
    const char *readPtr() const { return _data.data() + _head; }
    std::size_t contiguousReadable() const;
    void consume(std::size_t len);

    // Contiguous free space after the tail (for a single read() call).
    char *writePtr() { return _data.data() + tailIndex(); }
    std::size_t contiguousWritable() const;
    void commit(std::size_t len);

private:
    std::vector<char> _data;
    std::size_t _head = 0;
    std::size_t _size = 0;

    std::size_t tailIndex() const { return (_head + _size) % _data.size(); }
};

} // namespace zappy::net
