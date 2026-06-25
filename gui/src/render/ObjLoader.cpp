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

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        std::string token;
        ss >> token;

        if (token == "v") {
            float x, y, z; ss >> x >> y >> z;
            positions.push_back({x, y, z});
            float r, g, b;
            if (ss >> r >> g >> b) vertColors.push_back({r, g, b});
            else                   vertColors.push_back({1.f, 1.f, 1.f});
        } else if (token == "vn") {
            float x, y, z;
            ss >> x >> y >> z;
            normals.push_back({x, y, z});
        } else if (token == "f") {
            // Collect all corners in this face polygon
            std::vector<FaceCorner> polygon;
            std::string cornerStr;
            while (ss >> cornerStr) {
                FaceCorner fc{-1, -1};
                // Formats: v, v//vn, v/vt/vn, v/vt
                auto p1 = cornerStr.find('/');
                if (p1 == std::string::npos) {
                    fc.pi = std::stoi(cornerStr) - 1;
                } else {
                    fc.pi = std::stoi(cornerStr.substr(0, p1)) - 1;
                    auto p2 = cornerStr.find('/', p1+1);
                    if (p2 == std::string::npos) {
                        // v/vt — no normal
                    } else {
                        std::string nStr = cornerStr.substr(p2+1);
                        if (!nStr.empty()) {
                            fc.ni = std::stoi(nStr) - 1;
                        }
                    }
