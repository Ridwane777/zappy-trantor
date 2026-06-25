#pragma once
#include <glm/glm.hpp>
#include <vector>

class Shader;

// One active incantation ritual: a glowing dashed domain ring + rising light
// pillars, fading in/out over INCANT_DUR seconds.
struct IncantationEffect {
    glm::vec3 pos;
    float     timer    = 0.f;
    bool      success  = false;
};
constexpr float INCANT_DUR = 6.f;

// Draws every active ritual (dashed Fresnel ring + Gaussian-beam pillars).
// Self-contained GL state; uses the `incant` shader and the shared ring/quad
// meshes. No-op when fx is empty.
void drawIncantations(const std::vector<IncantationEffect>& fx, Shader& incant,
                      const glm::mat4& view, const glm::mat4& proj,
                      const glm::vec3& camPos, float elapsed);
