#include "GUIException.hpp"
#include "render/ObjLoader.hpp"
#include "AssetCache.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <array>
#include <tuple>
#include <unordered_map>
#include <stdexcept>
#include <cmath>

// Interleave pos + normal into flat float buffer and build index list.
// Keyed by (pos_idx, nrm_idx) pair.
struct VertKey {
    int pi, ni;
    bool operator==(const VertKey& o) const { return pi==o.pi && ni==o.ni; }
};
struct VertKeyHash {
    std::size_t operator()(const VertKey& k) const {
        return std::hash<long long>()(((long long)k.pi << 32) | (unsigned)k.ni);
    }
};

Mesh ObjLoader::load(const std::string& path) {
    std::string src = zappy::assetBytes(path);   // local or streamed from CDN (no disk write)
    if (src.empty())
        throw GUIException("ObjLoader: cannot open " + path);
    std::istringstream file(src);

    std::vector<std::array<float,3>> positions;
    std::vector<std::array<float,3>> normals;
    std::vector<std::array<float,3>> vertColors;  // per-vertex RGB baked color (optional)

    // Parsed face corners: (pos_idx, nrm_idx)  — -1 means absent
    struct FaceCorner { int pi, ni; };
    std::vector<std::array<FaceCorner,3>> faces;
