#include "GUIException.hpp"
#include "render/TexturedMesh.hpp"
#include "AssetCache.hpp"

#include <GL/glew.h>
#include <SFML/Graphics/Image.hpp>

#include <fstream>
#include <sstream>
#include <vector>
#include <array>
#include <unordered_map>
#include <stdexcept>
#include <cmath>

// ── OBJ parsing (pos + uv + normal) ──────────────────────────────────────────
namespace {

struct Key {
    int v, vt, vn;
    bool operator==(const Key &o) const { return v == o.v && vt == o.vt && vn == o.vn; }
};
struct KeyHash {
    std::size_t operator()(const Key &k) const {
        return (std::hash<int>()(k.v) * 73856093) ^
               (std::hash<int>()(k.vt) * 19349663) ^
               (std::hash<int>()(k.vn) * 83492791);
    }
};

// Parse one face-corner token "v/vt/vn" (any component may be absent).
Key parseCorner(const std::string &s)
{
    Key k{0, 0, 0};
    int field = 0;
    std::string cur;
    auto flush = [&]() {
        int val = cur.empty() ? 0 : std::atoi(cur.c_str());
        if (field == 0) k.v = val;
        else if (field == 1) k.vt = val;
        else k.vn = val;
        cur.clear();
    };
    for (char c : s) {
        if (c == '/') { flush(); ++field; }
        else cur += c;
    }
    flush();
    return k;
}

} // namespace
