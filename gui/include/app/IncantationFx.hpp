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

