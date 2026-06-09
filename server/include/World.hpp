#pragma once
#include "Map.hpp"
#include "Player.hpp"
#include "Args.hpp"
#include <vector>
#include <string>
#include <functional>

struct Egg {
    int id;
    int x;
    int y;
    std::string team;
};

class World {
public:
    explicit World(const ServerConfig &config);
    void update(int ticks);
    std::string executeCommand(int playerFd, const std::string &cmd);
    void addPlayer(int fd, const std::string &team);
    void removePlayer(int fd);

    // Callback que Server injecte pour envoyer aux GUIs
    std::function<void(const std::string &)> onGuiEvent;

private:
    Map _map;
    std::vector<Player> _players;
    std::vector<Egg> _eggs;
    ServerConfig _config;
    int _spawnTimer = 0;
    int _nextEggId  = 0;

    Player *_getPlayer(int fd);
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
