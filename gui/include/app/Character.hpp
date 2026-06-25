#pragma once
#include <glm/glm.hpp>
#include "render/SkinnedMesh.hpp"

// A standalone skinned character placed on the island (the two demo NPCs:
// the Huntress and the Barefoot Survivor). Distinct from the live/demo
// *players* — these idle, occasionally lie down, and glow when the Tree of
// Life's wave reaches them.
enum class CharState { IDLE, FALLING, GROUNDED, RISING };

struct Character {
    SkinnedMesh mesh;
    glm::vec3   color;               // top zone  (skin/head)
    glm::vec3   color2 = color;      // mid zone  (outfit)
    glm::vec3   color3 = color;      // bot zone  (legs/feet skin)
    float       split1 = 0.f;        // world Y top/mid boundary
    float       split2 = 0.f;        // world Y mid/bot boundary
    glm::vec3   worldPos;
    float       rotY;
    float       scale      = 1.f;
    int         idleAnim   = 0;
