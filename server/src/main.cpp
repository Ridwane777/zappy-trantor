#include "Args.hpp"
#include <iostream>

int main(int argc, char **argv)
{
    ServerConfig config = parseArgs(argc, argv);

    std::cout << "port:    " << config.port << "\n";
    std::cout << "width:   " << config.width << "\n";
    std::cout << "height:  " << config.height << "\n";
    std::cout << "clients: " << config.clients << "\n";
    std::cout << "freq:    " << config.freq << "\n";
    std::cout << "teams:   ";
    for (const auto &t : config.teams)
        std::cout << t << " ";
    std::cout << "\n";
    return 0;
}
