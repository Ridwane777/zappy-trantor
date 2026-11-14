#include "Args.hpp"
#include "util/Parse.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <set>

namespace zappy {

namespace {

constexpr int MAX_SIZE = 42;
constexpr int MAX_FREQ = 10000;

int readNumber(const std::vector<std::string> &args, std::size_t &i, const std::string &flag)
{
    if (i + 1 >= args.size())
        throw ArgsError(flag + " needs a value");
    auto value = parseInt(args[++i]);
    if (!value)
        throw ArgsError(flag + " expects a number, got \"" + args[i] + "\"");
    return static_cast<int>(*value);
}

int clamp(int value, int max, const std::string &what)
{
    if (value > max) {
        std::cerr << "Warning: " << what << " clamped to " << max << "\n";
        return max;
    }
    return value;
}

void checkTeams(const std::vector<std::string> &teams)
{
    if (teams.empty())
        throw ArgsError("-n needs at least one team name");
    std::set<std::string> seen;
    for (const auto &team : teams) {
        if (team.empty())
            throw ArgsError("team names cannot be empty");
        if (team == "GRAPHIC")
            throw ArgsError("\"GRAPHIC\" is reserved for graphical clients");
        if (std::any_of(team.begin(), team.end(), [](unsigned char c) { return std::isspace(c); }))
            throw ArgsError("team name \"" + team + "\" contains a space");
        if (!seen.insert(team).second)
            throw ArgsError("team \"" + team + "\" is given twice");
    }
}

} // namespace

ServerConfig parseArgs(const std::vector<std::string> &args)
{
    ServerConfig config;
    bool teamsGiven = false;

    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string &arg = args[i];
        if (arg == "-help" || arg == "--help" || arg == "-h") {
            config.help = true;
        } else if (arg == "-p") {
            config.port = readNumber(args, i, arg);
        } else if (arg == "-x") {
            config.width = readNumber(args, i, arg);
        } else if (arg == "-y") {
            config.height = readNumber(args, i, arg);
        } else if (arg == "-c") {
            config.clients = readNumber(args, i, arg);
        } else if (arg == "-f") {
            config.freq = readNumber(args, i, arg);
        } else if (arg == "-s") {
            int seed = readNumber(args, i, arg);
            if (seed < 0)
                throw ArgsError("-s must be >= 0");
            config.seed = static_cast<unsigned>(seed);
        } else if (arg == "-n") {
            if (!teamsGiven)
                config.teams.clear();
            teamsGiven = true;
            while (i + 1 < args.size() && (args[i + 1].empty() || args[i + 1][0] != '-'))
                config.teams.push_back(args[++i]);
            if (config.teams.empty())
                throw ArgsError("-n needs at least one team name");
        } else {
            throw ArgsError("unknown option \"" + arg + "\"");
        }
    }

    if (config.port < 1 || config.port > 65535)
        throw ArgsError("-p must be between 1 and 65535");
    if (config.width < 1 || config.height < 1)
        throw ArgsError("-x and -y must be > 0");
    if (config.clients < 1)
        throw ArgsError("-c must be > 0");
    if (config.freq < 1)
        throw ArgsError("-f must be > 0");
    config.width = clamp(config.width, MAX_SIZE, "width");
    config.height = clamp(config.height, MAX_SIZE, "height");
    config.freq = clamp(config.freq, MAX_FREQ, "frequency");
    checkTeams(config.teams);
    return config;
}

std::string usage(const std::string &bin)
{
    return "USAGE: " + bin + " [-p port] [-x width] [-y height] [-n name1 name2 ...] [-c clientsNb] [-f freq] [-s seed]\n"
           "  -p  port number (default 4242)\n"
           "  -x  width of the world (default 10, max 42)\n"
           "  -y  height of the world (default 10, max 42)\n"
           "  -n  team names (default Team1 Team2 Team3 Team4), unique and non-empty\n"
           "  -c  number of authorized clients per team (default 3)\n"
           "  -f  reciprocal of time unit for execution of actions (default 100)\n"
           "  -s  random seed, for reproducible games (default: random)\n";
}

} // namespace zappy
