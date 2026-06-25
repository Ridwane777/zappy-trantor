#include "world/World.hpp"

#include <glm/gtc/type_ptr.hpp>

#include "render/ObjLoader.hpp"
#include "GUIException.hpp"
#include <json.hpp>
using json = nlohmann::json;

#include <iostream>
#include <fstream>
#include <memory>
#include <unordered_map>
#include <cmath>
#include <cstdlib>
#include <sys/stat.h>

// ── OBJ mesh cache ──────────────────────────────────────────────────────────
// Mesh is move-only (no default ctor), so store by unique_ptr.
static std::unordered_map<std::string, std::unique_ptr<Mesh>> g_meshes;

Mesh &getMesh(const std::string &model)
{
    auto it = g_meshes.find(model);
    if (it != g_meshes.end()) return *it->second;

    std::string path = std::string(ASSET_DIR) + model + ".obj";
    g_meshes[model] = std::make_unique<Mesh>(ObjLoader::load(path));
    std::cout << "Loaded " << model << "\n";
    return *g_meshes[model];
}

glm::vec3 colorFor(const std::string &n)
{
    if (n == "tile")                                   return {0.55f, 0.72f, 0.32f};
    if (n == "water")                                  return {0.18f, 0.48f, 0.80f};
    if (n.find("normaltree") != std::string::npos)     return {0.22f, 0.58f, 0.15f};
    if (n.find("mapletree")  != std::string::npos)     return {0.30f, 0.65f, 0.10f};
    if (n.find("palmtree")   != std::string::npos)     return {0.25f, 0.62f, 0.18f};
    if (n.find("bush")       != std::string::npos)     return {0.28f, 0.58f, 0.18f};
    if (n.find("grass")      != std::string::npos)     return {0.38f, 0.70f, 0.22f};
    if (n.find("plant")      != std::string::npos)     return {0.30f, 0.62f, 0.22f};
    if (n.find("flower_1")   != std::string::npos)     return {0.95f, 0.45f, 0.70f};
    if (n.find("flower_2")   != std::string::npos)     return {0.95f, 0.85f, 0.20f};
    if (n.find("rock")       != std::string::npos)     return {0.55f, 0.52f, 0.48f};
    if (n == "lily_pad")                               return {0.25f, 0.65f, 0.30f};
    if (n == "boat")                                   return {0.45f, 0.32f, 0.18f};
    if (n == "log")                                    return {0.50f, 0.32f, 0.15f};
    if (n == "cloud")                                  return {0.95f, 0.96f, 1.00f};
    if (n == "torch")                                  return {0.70f, 0.40f, 0.15f};
    if (n == "player_male")                            return {0.62f, 0.45f, 0.30f};
    if (n == "player_female")                          return {0.78f, 0.55f, 0.38f};
    if (n == "player_female2")                         return {0.70f, 0.50f, 0.34f};
    if (n.find("player")     != std::string::npos)     return {0.85f, 0.72f, 0.60f};
    float h = 0.f;
    for (char c : n) h = h * 31.f + c;
    h = std::fmod(std::abs(h) / 997.f, 1.f);
    float H6 = h * 6.f, C = 0.55f;
    float X = C * (1.f - std::abs(std::fmod(H6, 2.f) - 1.f));
    int s = (int)H6;
    glm::vec3 rgb;
    if      (s==0) rgb={C,X,0}; else if (s==1) rgb={X,C,0};
    else if (s==2) rgb={0,C,X}; else if (s==3) rgb={0,X,C};
    else if (s==4) rgb={X,0,C}; else           rgb={C,0,X};
    return rgb + glm::vec3(0.25f);
}

MapData loadMap(const std::string &path)
{
    std::ifstream f(path);
    if (!f.is_open()) throw GUIException("Cannot open " + path);
    json doc = json::parse(f);

    MapData m;
    m.w = doc["meta"]["width"];
    m.h = doc["meta"]["height"];

    auto &sc = doc["atmosphere"]["sky_color"];
    m.skyColor = {sc[0], sc[1], sc[2]};
    auto &fc = doc["atmosphere"]["fog_color"];
    m.fogColor = {fc[0], fc[1], fc[2]};

    for (auto &t : doc["tiles"]) {
        Tile tile;
        tile.x = t["x"]; tile.y = t["y"];
        tile.ground = t["ground"];
        for (auto &p : t["props"])
            tile.props.push_back({p["model"], p["dx"], p["dz"], p["rot_y"], p["scale"]});
        if (t.contains("resources")) {
            for (int i = 0; i < 7; ++i)
                if (t["resources"].contains(RES_NAMES[i]))
                    tile.res[i] = t["resources"][RES_NAMES[i]];
        }
        m.tiles.push_back(tile);
    }
    for (auto &c : doc["atmosphere"]["clouds"]) {
        float dx = c.contains("drift_x") ? (float)c["drift_x"] : (float)c["drift_speed"];
        float dz = c.contains("drift_z") ? (float)c["drift_z"] : 0.f;
        m.clouds.push_back({c["x"], c["y"], c["z"], c["scale"], dx, dz});
    }
    return m;
}

time_t fileMtime(const std::string &p)
{
    struct stat s; return (stat(p.c_str(), &s) == 0) ? s.st_mtime : 0;
}

Mesh buildShoreMesh(const std::vector<Tile> &tiles, int dist)
{
    std::unordered_map<int, std::string> groundOf;
    for (auto &t : tiles) groundOf[t.y * 512 + t.x] = t.ground;

    auto isLand = [&](int x, int y) {
        auto it = groundOf.find(y * 512 + x);
        return it != groundOf.end() && it->second == "tile";
    };

    std::unordered_map<int, int> distMap;
    for (auto &t : tiles) {
        if (t.ground != "water") continue;
        int key = t.y * 512 + t.x;
        int dx[] = {0,0,1,-1}; int dz[] = {1,-1,0,0};
        int minD = 999;
        for (int i = 0; i < 4; ++i)
            if (isLand(t.x + dx[i], t.y + dz[i])) { minD = 1; break; }
        distMap[key] = minD;
    }
    for (int d = 2; d <= dist + 1; ++d) {
        for (auto &t : tiles) {
            if (t.ground != "water") continue;
            int key = t.y * 512 + t.x;
            if (distMap[key] < d) continue;
            int dx[] = {0,0,1,-1}; int dz[] = {1,-1,0,0};
            for (int i = 0; i < 4; ++i) {
                int nk = (t.y + dz[i]) * 512 + (t.x + dx[i]);
                if (distMap.count(nk) && distMap[nk] == d - 1) { distMap[key] = d; break; }
            }
        }
    }

    std::vector<float> verts; std::vector<unsigned int> idx; unsigned int base = 0;
    for (auto &t : tiles) {
        if (t.ground != "water") continue;
        int d = distMap.count(t.y * 512 + t.x) ? distMap[t.y * 512 + t.x] : 999;
        if (d != dist) continue;
        float x0 = t.x * TILE - TILE * 0.501f, x1 = t.x * TILE + TILE * 0.501f;
