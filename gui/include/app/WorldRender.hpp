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
