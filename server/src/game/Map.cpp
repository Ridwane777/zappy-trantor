#include "game/Map.hpp"
#include "game/GameRules.hpp"
#include <stdexcept>

namespace zappy {

Map::Map(int width, int height, std::mt19937 &rng)
    : _width(width), _height(height), _rng(rng)
{
    if (width <= 0 || height <= 0)
        throw std::invalid_argument("map dimensions must be > 0");
    _tiles.resize(static_cast<std::size_t>(width) * height);
    spawnResources();
}

Position Map::wrap(Position p) const
{
    return {((p.x % _width) + _width) % _width, ((p.y % _height) + _height) % _height};
}

Tile &Map::tile(Position p)
{
    Position w = wrap(p);
    return _tiles[static_cast<std::size_t>(w.y) * _width + w.x];
}

const Tile &Map::tile(Position p) const
{
    Position w = wrap(p);
    return _tiles[static_cast<std::size_t>(w.y) * _width + w.x];
}

Position Map::randomPosition()
{
    std::uniform_int_distribution<int> dx(0, _width - 1);
    std::uniform_int_distribution<int> dy(0, _height - 1);
    int x = dx(_rng);
    return {x, dy(_rng)};
}

void Map::spawnResources()
{
    for (Resource r : ALL_RESOURCES) {
        int target = static_cast<int>(_width * _height * rules::DENSITY[index(r)]);
        int current = 0;
        for (const Tile &t : _tiles)
            current += t.count(r);
        for (int i = current; i < target; ++i)
            tile(randomPosition()).add(r);
    }
}

} // namespace zappy
