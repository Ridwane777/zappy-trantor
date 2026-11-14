#pragma once

#include "game/GameRules.hpp"
#include "game/Map.hpp"
#include "game/Resource.hpp"
#include <deque>
#include <string>

namespace zappy {

// Values match the GUI protocol (1 = north ... 4 = west).
enum class Orientation { North = 1, East = 2, South = 3, West = 4 };

struct Action {
    std::string name;   // command name, e.g. "Take"
    std::string arg;    // argument, e.g. "food" (empty when none)
    int remainingTicks;
    bool started = false;
    bool known = true;  // false: unknown command, answered "ko" in order
};

// A player as the game sees it: no socket, no protocol, only game state.
// All changes go through methods that keep the state valid.
class Player {
public:
    Player(int id, std::string team, Position pos, Orientation orientation);

    int id() const { return _id; }
    const std::string &team() const { return _team; }
    Position position() const { return _pos; }
    Orientation orientation() const { return _orientation; }
    int level() const { return _level; }
    bool isDead() const { return _dead; }
    bool isIncanting() const { return _incanting; }
    int count(Resource r) const { return _inventory[index(r)]; }
    const ResourceCounts &inventory() const { return _inventory; }

    void moveTo(Position pos) { _pos = pos; }
    void turnRight();
    void turnLeft();
    void addItem(Resource r) { ++_inventory[index(r)]; }
    bool removeItem(Resource r);
    void levelUp();
    void setIncanting(bool incanting) { _incanting = incanting; }
    void kill() { _dead = true; }

    enum class Hunger { Nothing, Ate, Starved };
    // Advances the food clock by one time unit. Food is not consumed while incanting.
    Hunger tickFood();

    // Command queue (at most rules::MAX_PENDING_COMMANDS).
    bool enqueue(Action action);
    bool hasAction() const { return !_actions.empty(); }
    Action &currentAction() { return _actions.front(); }
    void finishAction() { _actions.pop_front(); }

private:
    int _id;
    std::string _team;
    Position _pos;
    Orientation _orientation;
    int _level = rules::MIN_LEVEL;
    bool _dead = false;
    bool _incanting = false;
    int _foodTimer = rules::FOOD_DURATION;
    ResourceCounts _inventory{};
    std::deque<Action> _actions;
};

} // namespace zappy
