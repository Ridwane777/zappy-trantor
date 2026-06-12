#pragma once
#include "Map.hpp"
#include "Player.hpp"
#include "Args.hpp"
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
struct Egg {
    int id;
    int x;
    int y;
    std::string team;
};
using CommandHandler = std::function<std::string(Player &, const std::string &)>;

class World {
public:
    explicit World(const ServerConfig &config);

    void        update(int ticks);
    std::string executeCommand(int playerFd, const std::string &cmd);
    void        addPlayer(int fd, const std::string &team);
    void        removePlayer(int fd);
    std::vector<std::string> getGuiSnapshot() const;

    std::function<void(const std::string &)> onGuiEvent;
    std::function<void(int fd, const std::string &)> onPlayerEvent;

private:
    Map _map;
    std::vector<Player> _players;
    std::vector<Egg> _eggs;
    ServerConfig _config;
    int _spawnTimer = 0;
    int _nextEggId  = 0;

    std::unordered_map<std::string, CommandHandler> _handlers;
    void _initHandlers();

    Player     *_getPlayer(int fd);

    std::string _cmdForward(Player &p);
    std::string _cmdRight(Player &p);
    std::string _cmdLeft(Player &p);
    std::string _cmdLook(Player &p);
    std::string _cmdInventory(Player &p);
    std::string _cmdTake(Player &p, const std::string &res);
    std::string _cmdSet(Player &p, const std::string &res);
    std::string _cmdBroadcast(Player &p, const std::string &msg);
    std::string _cmdFork(Player &p);
    std::string _cmdEject(Player &p);
    std::string _cmdIncantation(Player &p);
    std::string _cmdConnectNbr(Player &p);
};