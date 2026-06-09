#pragma once 
#include "Tile.hpp"
#include <array>
#include <string>
 
enum Orientation {
    NORTH = 1,
    EAST  = 2,
    SOUTH = 3,
    WEST  = 4
};
 
struct Player {
    int         id;
    int         fd;
    int         x;
    int         y;
    Orientation orientation;
    int         level;
    bool        isDead;
    bool        isIncanting;
    std::array<int, RESOURCE_COUNT> inventory{};
    std::string team;
 
    Player(int id, int fd, int x, int y, const std::string &team);
 
    void turnRight();
    void turnLeft();
};
