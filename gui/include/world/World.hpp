#pragma once
// World data + mesh helpers for the GUI: the map.json data model (tiles, props,
// clouds), the per-tile/resource constants, the OBJ mesh cache, and the
// procedural meshes (ground, shore, ring, gem, …). Used by the game loop.

#include <glm/glm.hpp>
#include <ctime>
#include <string>
#include <vector>

#include "render/Mesh.hpp"
#include "render/Shader.hpp"

inline constexpr float TILE = 2.0f;             // world units per tile
inline const char *const MAP_FILE  = "map.json";
inline const char *const ASSET_DIR = "assets/models/";

// resource indices: 0=food 1=linemate 2=deraumere 3=sibur 4=mendiane 5=phiras 6=thystame
inline const char *RES_NAMES[7] = {"food", "linemate", "deraumere", "sibur",
                                   "mendiane", "phiras", "thystame"};
inline const glm::vec3 RES_COLORS[7] = {
    {1.00f, 0.55f, 0.10f},  // food      — orange
    {0.75f, 0.88f, 1.00f},  // linemate  — silver
    {0.60f, 0.18f, 0.90f},  // deraumere — purple
    {0.10f, 0.90f, 0.78f},  // sibur     — teal
    {0.25f, 0.52f, 1.00f},  // mendiane  — blue
    {1.00f, 0.82f, 0.10f},  // phiras    — gold
    {1.00f, 0.20f, 0.62f},  // thystame  — pink
};

struct Prop  { std::string model; float dx, dz, rot_y, scale; };
struct Tile  { int x, y; std::string ground; std::vector<Prop> props; int res[7] = {}; };
struct Cloud { float x, y, z, scale, drift_x, drift_z; };

struct MapData {
    int w = 0, h = 0;
    std::vector<Tile>  tiles;
    std::vector<Cloud> clouds;
    glm::vec3 skyColor {0.44f, 0.70f, 0.94f};
    glm::vec3 fogColor {0.60f, 0.82f, 0.95f};
};

// OBJ mesh cache (loads on first use, prints once).
Mesh     &getMesh(const std::string &model);
glm::vec3 colorFor(const std::string &n);
MapData   loadMap(const std::string &path);
time_t    fileMtime(const std::string &p);

Mesh buildShoreMesh(const std::vector<Tile> &tiles, int dist);
Mesh buildGroundMesh(const std::vector<Tile> &tiles, const std::string &groundType,
                     bool cliffOnly = false);
Mesh buildGemMesh();
Mesh buildDashedRingMesh(int segs = 8, float fill = 0.55f);
Mesh buildVQuadMesh();
Mesh buildRingMesh();
float frnd(float a, float b);

void drawMesh(Shader &toon, Shader &unused, Mesh &mesh,
              const glm::mat4 &model, const glm::mat3 &normalMat, const glm::vec3 &color,
              const glm::mat4 &view, const glm::mat4 &proj,
              float elapsed = 0.f, bool sway = false);
