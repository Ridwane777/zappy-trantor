#pragma once
#include <glm/glm.hpp>
#include <memory>

class Shader;
class Mesh;
class TexturedMesh;
struct MapData;

// Static-world render passes pulled out of GuiApp's frame loop. The shared
// per-frame uniforms (view/proj/light/etc.) are set on `toon`/`egg` by the
// caller beforehand; these helpers only toggle pass-specific uniforms + draw.

// Seamless ground: one circular island per team (ring layout) on a global ocean,
// each with its own shore. teamCount <= 1 -> a single central island.
// `waterShader`/`waterTex` render the shore water with the same rippling
// texture + sun-glitter look as the intro screen's water plane (water_map.vert
// /.frag); pass waterTex == 0 to fall back to the old flat-colour `toon` water.
void drawGround(Shader& toon, Shader& waterShader, unsigned int waterTex,
                const glm::mat4& view, const glm::mat4& proj, float elapsed,
                const glm::vec2& sunXZ, Mesh& land, Mesh& ocean, Mesh& water,
                const MapData& map, float cx, float cz, int teamCount);

// Props (trees/plants/flowers + the vertex-coloured Tree of Life); vegetation
// sways, the tree's foliage glows with treeGlow. Drawn once per team island
// (full-size copy, translated) — teamCount <= 1 -> a single central island.
void drawProps(Shader& toon, Shader& outline, const MapData& map,
               const glm::mat4& view, const glm::mat4& proj,
               float elapsed, float ambScale, float treeGlow,
               float cx, float cz, int teamCount);

// Floating textured gemstones, one per stone type per tile (gemTex[1..6]).
// Same server resource grid duplicated onto each team island.
void drawResources(Shader& egg, const std::unique_ptr<TexturedMesh>* gemTex,
                   const MapData& map, const glm::vec3& sunDir,
                   const glm::vec3& lightCol, float ambScale,
                   const glm::mat4& view, const glm::mat4& proj, float elapsed,
                   float cx, float cz, int teamCount);

// Floating textured food (apple/grapes/mushroom), varied per tile (foodMesh[0..2]).
// Same server resource grid duplicated onto each team island.
void drawFood(Shader& egg, const std::unique_ptr<TexturedMesh>* foodMesh,
              const MapData& map, const glm::vec3& sunDir,
              const glm::vec3& lightCol, float ambScale,
              const glm::mat4& view, const glm::mat4& proj, float elapsed,
              float cx, float cz, int teamCount);

// Drifting clouds (toon-shaded, no outline).
void drawClouds(Shader& toon, const MapData& map, float elapsed);
