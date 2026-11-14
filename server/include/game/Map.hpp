#pragma once

#include "game/Tile.hpp"
#include <random>
#include <vector>

namespace zappy {

struct Position {
    int x;
    int y;
    bool operator==(const Position &o) const { return x == o.x && y == o.y; }
    bool operator!=(const Position &o) const { return !(*this == o); }
};

// Toroidal grid of tiles. Coordinates wrap around both edges.
class Map {
public:
    Map(int width, int height, std::mt19937 &rng);

    int width() const { return _width; }
    int height() const { return _height; }

    Tile &tile(Position p);
    const Tile &tile(Position p) const;
    Position wrap(Position p) const;
    Position randomPosition();

    // Tops every resource up to its target density (rules::DENSITY), at random tiles.
    void spawnResources();

private:
    int _width;
    int _height;
    std::mt19937 &_rng;
    std::vector<Tile> _tiles;
};

} // namespace zappy
