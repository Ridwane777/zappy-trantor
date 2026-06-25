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

// Seamless ground: circular-clipped land + ocean/shore gradient + animated water.
void drawGround(Shader& toon, Mesh& land, Mesh& ocean, Mesh& water,
                const MapData& map, float cx, float cz);

// Props (trees/plants/flowers + the vertex-coloured Tree of Life); vegetation
// sways, the tree's foliage glows with treeGlow.
void drawProps(Shader& toon, Shader& outline, const MapData& map,
               const glm::mat4& view, const glm::mat4& proj,
               float elapsed, float ambScale, float treeGlow);

// Floating textured gemstones, one per stone type per tile (gemTex[1..6]).
void drawResources(Shader& egg, const std::unique_ptr<TexturedMesh>* gemTex,
                   const MapData& map, const glm::vec3& sunDir,
                   const glm::vec3& lightCol, float ambScale,
