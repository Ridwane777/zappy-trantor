#include "app/WorldRender.hpp"
#include "render/Shader.hpp"
#include "render/Mesh.hpp"
#include "render/TexturedMesh.hpp"
#include "world/World.hpp"        // TILE, MapData, getMesh, colorFor, drawMesh

#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <string>

void drawGround(Shader& toon, Mesh& land, Mesh& ocean, Mesh& water,
                const MapData& map, float cx, float cz)
{
    glm::mat4 I(1.f); glm::mat3 N3(1.f);
    toon.use();
    toon.setMat4("uModel",     glm::value_ptr(I));
    toon.setMat3("uNormalMat", glm::value_ptr(N3));
    toon.setBool("uEmissive",  false);

    glDisable(GL_CULL_FACE);
    toon.setFloat("uAlpha", 1.0f);
    toon.setBool ("uCircleClip", false);

    // Circular clip so land has smooth round edges, no square corners
    float islandR = map.w * TILE * 0.455f;
    toon.setBool ("uCircleClip", true);
    toon.setVec2 ("uClipCenter", cx, cz);
    toon.setFloat("uClipRadius", islandR);
    toon.setVec3("uColor", 0.52f, 0.70f, 0.28f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    land.draw();
    glDisable(GL_BLEND);
    toon.setBool("uCircleClip", false);

    // Ocean + shore — continuous radial gradient, smooth circular fade
    float shoreInnerR = islandR * 0.88f;
    float shoreWidth  = TILE * 5.0f;
    toon.setVec2 ("uClipCenter",   cx, cz);
    toon.setBool ("uShoreBlend",   true);
    toon.setFloat("uShoreInnerR",  shoreInnerR);
    toon.setFloat("uShoreWidth",   shoreWidth);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Static deep ocean background
    toon.setBool("uWave", false);
    toon.setVec3("uColor", 0.06f, 0.22f, 0.55f);
    ocean.draw();

    // Animated near-shore water
    toon.setBool("uWave", true);
    water.draw();
    toon.setBool("uWave", false);

    glDisable(GL_BLEND);
    toon.setBool("uShoreBlend", false);
    glEnable(GL_CULL_FACE);
}

void drawProps(Shader& toon, Shader& outline, const MapData& map,
