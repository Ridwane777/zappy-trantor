#pragma once

#include <stdexcept>
#include <string>
#include <vector>

namespace zappy {

// Every option has a default, so `./zappy_server` alone starts a playable game.
struct ServerConfig {
    int port = 4242;
    int width = 10;
    int height = 10;
    int clients = 3;
    int freq = 100;
    std::vector<std::string> teams = {"Team1", "Team2", "Team3", "Team4"};
    unsigned seed = 0;       // 0: a random seed
    bool help = false;
};

class ArgsError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Parses the command line. Throws ArgsError with a readable message on any
// invalid input; out-of-range sizes and frequency are clamped with a warning.
ServerConfig parseArgs(const std::vector<std::string> &args);

std::string usage(const std::string &bin);

} // namespace zappy
