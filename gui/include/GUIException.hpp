#pragma once
// Error type for the GUI component. All GUI-side failures throw this so callers
// can catch one type (and distinguish GUI errors from std::exception in general).

#include <exception>
#include <string>
#include <utility>

class GUIException : public std::exception {
public:
    explicit GUIException(std::string message) : _message(std::move(message)) {}

    const char *what() const noexcept override { return _message.c_str(); }

private:
    std::string _message;
};
