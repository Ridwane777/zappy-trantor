#include "Args.hpp"
#include "app/GameSession.hpp"
#include "game/World.hpp"
#include "net/TcpServer.hpp"
#include <atomic>
#include <csignal>
#include <iostream>
#include <random>

namespace {

std::atomic<bool> g_stop{false};

void requestStop(int) { g_stop = true; }

} // namespace

int main(int argc, char **argv)
{
    using namespace zappy;

    ServerConfig config;
    try {
        config = parseArgs(std::vector<std::string>(argv, argv + argc));
    } catch (const ArgsError &e) {
        std::cerr << "Error: " << e.what() << "\n" << usage(argv[0]);
        return 84;
    }
    if (config.help) {
        std::cout << usage(argv[0]);
        return 0;
    }

    try {
        unsigned seed = config.seed ? config.seed : std::random_device{}();
        net::TcpServer network(config.port);
        app::GameSession session(network, config.freq);
        World world({config.width, config.height, config.teams, config.clients, seed}, session);
        session.attach(world);
        network.setHandler(session);

        std::signal(SIGINT, requestStop);
        std::signal(SIGTERM, requestStop);
        std::cout << "Server listening on port " << config.port << " (seed " << seed << ")" << std::endl;

        while (!g_stop) {
            session.advanceClock();
            network.pollOnce(session.msUntilNextTick());
        }
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 84;
    }
    return 0;
}
