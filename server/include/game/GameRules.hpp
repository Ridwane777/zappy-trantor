#pragma once

#include "game/Resource.hpp"
#include <array>
#include <cstddef>
#include <string_view>

// Every number that defines the game, in one place.
namespace zappy::rules {

constexpr int FOOD_DURATION = 126;      // time units one food unit keeps a player alive
constexpr int START_FOOD = 10;          // food in a new player's inventory
constexpr int MAX_PENDING_COMMANDS = 10;
constexpr int RESPAWN_PERIOD = 20;      // time units between two resource top-ups
constexpr int MIN_LEVEL = 1;
constexpr int MAX_LEVEL = 8;
constexpr int WINNING_PLAYERS = 6;      // players of one team at MAX_LEVEL to win

// Target amount of each resource on the map, as a fraction of the tile count.
constexpr std::array<double, RESOURCE_COUNT> DENSITY = {
    0.5, 0.3, 0.15, 0.1, 0.1, 0.08, 0.05,
};

struct ElevationRequirement {
    int players;
    ResourceCounts stones;   // index 0 (food) is always 0
};

// ELEVATION[level - 1]: what an incantation from `level` to `level + 1` needs.
constexpr std::array<ElevationRequirement, MAX_LEVEL - 1> ELEVATION = {{
    {1, {0, 1, 0, 0, 0, 0, 0}},
    {2, {0, 1, 1, 1, 0, 0, 0}},
    {2, {0, 2, 0, 1, 0, 2, 0}},
    {4, {0, 1, 1, 2, 0, 1, 0}},
    {4, {0, 1, 2, 1, 3, 0, 0}},
    {6, {0, 1, 2, 3, 0, 1, 0}},
    {6, {0, 2, 2, 2, 2, 2, 1}},
}};

constexpr const ElevationRequirement &elevation(int level) { return ELEVATION[level - MIN_LEVEL]; }

struct CommandSpec {
    std::string_view name;
    int cost;            // time units
    bool takesArgument;
};

constexpr std::array<CommandSpec, 12> COMMANDS = {{
    {"Forward", 7, false},
    {"Right", 7, false},
    {"Left", 7, false},
    {"Look", 7, false},
    {"Inventory", 1, false},
    {"Broadcast", 7, true},
    {"Connect_nbr", 0, false},
    {"Fork", 42, false},
    {"Eject", 7, false},
    {"Take", 7, true},
    {"Set", 7, true},
    {"Incantation", 300, false},
}};

} // namespace zappy::rules
