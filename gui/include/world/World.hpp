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
