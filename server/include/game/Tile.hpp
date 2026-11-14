#pragma once

#include "game/Resource.hpp"

namespace zappy {

class Tile {
public:
    int count(Resource r) const { return _resources[index(r)]; }
    const ResourceCounts &resources() const { return _resources; }

    void add(Resource r, int n = 1) { _resources[index(r)] += n; }

    // Removes one unit; false (and no change) when there is none.
    bool remove(Resource r)
    {
        int &n = _resources[index(r)];
        if (n <= 0)
            return false;
        --n;
        return true;
    }

private:
    ResourceCounts _resources{};
};

} // namespace zappy
