#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include "render/SkinnedMesh.hpp"

class Shader;
class GuiClient;
class HudOverlay;
class Constellation;
class AudioManager;
class TexturedMesh;
struct GuiPlayer;
struct MapData;
struct Spark;

// One skinned mesh per 2-level band (1-2,3-4,5-6,7-8), shared by every player
// in that band.
struct PlayerTier { SkinnedMesh mesh; float scale = 0.05f; bool loaded = false; };

// Tier index for a level (0=lvl1-2 ... 3=lvl7-8); falls back to a loaded tier.
