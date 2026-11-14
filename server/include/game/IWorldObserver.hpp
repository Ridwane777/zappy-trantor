#pragma once

#include "game/Map.hpp"
#include "game/Resource.hpp"
#include <string>
#include <vector>

namespace zappy {

class Player;
struct Egg;

// What the game reports to the outside world. The game layer only knows this
// interface: it never touches sockets or GUI protocol text.
class IWorldObserver {
public:
    virtual ~IWorldObserver() = default;

    // Reply or asynchronous message for one AI player (one protocol line, no '\n').
    virtual void onReply(int playerId, const std::string &line) = 0;
    virtual void onPlayerDied(const Player &p) = 0;

    virtual void onPlayerJoined(const Player &p) = 0;
    virtual void onPlayerMoved(const Player &p) = 0;
    virtual void onPlayerLevel(const Player &p) = 0;
    virtual void onPlayerInventory(const Player &p) = 0;
    virtual void onPlayerLeft(const Player &p) = 0;
    virtual void onResourceTaken(const Player &p, Resource r) = 0;
    virtual void onResourceDropped(const Player &p, Resource r) = 0;
    virtual void onTileChanged(Position pos) = 0;
    virtual void onPlayerExpelled(const Player &p) = 0;
    virtual void onBroadcast(const Player &p, const std::string &message) = 0;
    virtual void onIncantationStart(Position pos, int level, const std::vector<int> &playerIds) = 0;
    virtual void onIncantationEnd(Position pos, bool success) = 0;
    virtual void onEggLaid(const Player &p, const Egg &egg) = 0;
    virtual void onEggHatched(const Egg &egg) = 0;
    virtual void onEggDestroyed(const Egg &egg) = 0;
    virtual void onGameWon(const std::string &team) = 0;
};

} // namespace zappy
