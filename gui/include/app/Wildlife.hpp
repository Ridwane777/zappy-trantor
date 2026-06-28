#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "render/SkinnedMesh.hpp"
#include "render/Shader.hpp"

struct WildlifeEntity {
    glm::vec3 pos;
    glm::vec3 target;
    float     rotY     = 0.f;
    float     speed    = 1.f;
    float     animTime = 0.f;
    float     minY, maxY;
};

struct WildlifeGroup {
    SkinnedMesh              mesh;
    std::vector<WildlifeEntity> entities;
    float                    scale     = 1.f;
    float                    baseTilt  = 0.f; // degrees, pitch about local X applied to every entity
    float                    baseRoll  = 0.f; // degrees, roll about local Z (stand flat wings upright)
    bool                     alphaCutout = false; // texture alpha defines the silhouette (butterfly wings)
};

// Roam within a disk of `radius` around (cx, cz) — the island cluster — so the
// wildlife stays over land instead of wandering the surrounding ocean.
void updateWildlife(WildlifeGroup &g, float dt, float cx, float cz, float radius);

void drawWildlife(WildlifeGroup &g, Shader &skin,
                  const glm::mat4 &view, const glm::mat4 &proj,
                  const glm::vec3 &camPos,
                  const glm::vec3 &sunDir, const glm::vec3 &lightCol,
                  float ambScale, float elapsed);
