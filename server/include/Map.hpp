
#pragma once
#include "Tile.hpp"
#include <vector>
 
class Map {
public:
    Map(int width, int height);
 
    Tile &getTile(int x, int y);
    const Tile &getTile(int x, int y) const;
    int wrapX(int x) const;
    int wrapY(int y) const;
    void spawnResources();

    int getWidth()  const {
        return _width;
    }
    int getHeight() const {
        return _height;
    }
 
private:
    int _width;
    int _height;
    std::vector<std::vector<Tile>> _tiles;
    void _spawnResource(Resource r, double density);
};
