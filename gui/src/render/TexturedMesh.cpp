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

// ── lifecycle ────────────────────────────────────────────────────────────────

TexturedMesh::~TexturedMesh() { cleanup(); }

void TexturedMesh::cleanup()
{
    if (_ebo) glDeleteBuffers(1, &_ebo);
    if (_vbo) glDeleteBuffers(1, &_vbo);
    if (_vao) glDeleteVertexArrays(1, &_vao);
    if (_texId) glDeleteTextures(1, &_texId);
    _vao = _vbo = _ebo = _texId = 0;
}

TexturedMesh::TexturedMesh(TexturedMesh &&o) noexcept
{
    *this = std::move(o);
}

TexturedMesh &TexturedMesh::operator=(TexturedMesh &&o) noexcept
{
    if (this != &o) {
        cleanup();
        _vao = o._vao; _vbo = o._vbo; _ebo = o._ebo; _texId = o._texId;
        _indexCount = o._indexCount; _centroid = o._centroid; _radius = o._radius;
        o._vao = o._vbo = o._ebo = o._texId = 0;
        o._indexCount = 0;
    }
    return *this;
}

// ── loading ──────────────────────────────────────────────────────────────────

TexturedMesh TexturedMesh::load(const std::string &objPath, const std::string &texPath)
{
    std::string objSrc = zappy::assetBytes(objPath);   // local or streamed from CDN
    if (objSrc.empty())
        throw GUIException("TexturedMesh: cannot open " + objPath);
    std::istringstream f(objSrc);

    std::vector<std::array<float, 3>> positions;
    std::vector<std::array<float, 2>> uvs;
    std::vector<std::array<float, 3>> normals;

    std::vector<float>        verts;     // interleaved pos3 + nrm3 + uv2
    std::vector<unsigned int> indices;
    std::unordered_map<Key, unsigned int, KeyHash> seen;

    auto resolve = [&](int idx, int n) { return idx < 0 ? n + idx : idx - 1; };

    std::string line;
    while (std::getline(f, line)) {
        if (line.size() < 2) continue;
