#pragma once

#include "game/IWorldObserver.hpp"
#include "game/Map.hpp"
#include "game/Player.hpp"
#include <optional>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace zappy {

struct Egg {
    int id;
    Position pos;
    std::string team;
};

struct WorldConfig {
    int width;
    int height;
    std::vector<std::string> teams;
    int slotsPerTeam;
    unsigned seed;       // random seed: same seed, same map and spawns
};

// The game rules. Players are identified by an id; the world knows nothing
// about connections and reports everything through IWorldObserver.
class World {
public:
    World(const WorldConfig &config, IWorldObserver &observer);

    struct Join {
        int playerId;
        int freeSlots;   // slots left for the team after this join
    };
    // A new AI client wants to play for `team`: takes an initial slot, otherwise
    // hatches one of the team's eggs. Nothing for an unknown or full team.
    std::optional<Join> join(const std::string &team);
    void leave(int playerId);

    // Queues one command line of a player. False when the queue is full (the
    // line is dropped without reply, as the protocol requires).
    bool queueCommand(int playerId, const std::string &line);

    // Advances the game by `ticks` time units.
    void update(int ticks);

    const Map &map() const { return _map; }
    const std::vector<std::string> &teams() const { return _config.teams; }
    const std::vector<Player> &players() const { return _players; }
    const std::vector<Egg> &eggs() const { return _eggs; }
    const Player *findPlayer(int id) const;

private:
    WorldConfig _config;
    IWorldObserver &_observer;
    std::mt19937 _rng;
    Map _map;
    std::vector<Player> _players;
    std::vector<Egg> _eggs;
    std::unordered_map<std::string, int> _freeSlots;
    int _nextPlayerId = 1;
    int _nextEggId = 1;
    int _spawnTimer = 0;
    bool _gameOver = false;

    Player *find(int id);
    int freeSlotsFor(const std::string &team) const;   // initial slots + eggs
    void tick();
    void tickPlayer(Player &p);
    void execute(Player &p, const Action &a);
    int playersReadyFor(const Player &p) const;
    bool meetsElevation(const Player &p) const;
    void checkVictory();

    std::string cmdForward(Player &p);
    std::string cmdTurn(Player &p, bool right);
    std::string cmdLook(const Player &p) const;
    std::string cmdInventory(const Player &p) const;
    std::string cmdTake(Player &p, const std::string &arg);
    std::string cmdSet(Player &p, const std::string &arg);
    std::string cmdBroadcast(Player &p, const std::string &message);
    std::string cmdConnectNbr(const Player &p) const;
    std::string cmdFork(Player &p);
    std::string cmdEject(Player &p);
    bool startIncantation(Player &p);
    std::string endIncantation(Player &p);
};

// Direction (0 = same tile, 1 = front, then clockwise to 8) from which `receiver`
// hears a sound emitted at `from`, taking the shortest way on the torus.
int soundDirection(Position from, const Player &receiver, int width, int height);

} // namespace zappy
