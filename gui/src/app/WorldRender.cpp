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
               const glm::mat4& view, const glm::mat4& proj,
               float elapsed, float ambScale, float treeGlow)
{
    static const std::string kSwayModels[] = {
        "normaltree_1","mapletree_1","palmtree_1","plant_1",
        "grass_large","flower_1","flower_2","bush"
    };
    auto isVegetation = [&](const std::string& m) {
        for (auto& s : kSwayModels) if (m.find(s) != std::string::npos) return true;
        return false;
    };

    for (auto& t : map.tiles) {
        float wx = t.x * TILE;
        float wz = t.y * TILE;
        for (auto& p : t.props) {
            Mesh& pm = getMesh(p.model);
            float r  = pm.boundingRadius();
            float s  = (r > 1e-4f) ? p.scale / r : p.scale;
            // roses (flowers) shrunk to ~resource size
            if (p.model == "flower_1" || p.model == "flower_2")
                s = (r > 1e-4f) ? 0.25f / r : 0.25f;

            glm::mat4 M = glm::translate(glm::mat4(1.f),
                {wx + p.dx * TILE, 0.f, wz + p.dz * TILE});
            M = glm::rotate(M, glm::radians(p.rot_y), {0.f, 1.f, 0.f});
            M = glm::scale(M, glm::vec3(s));
            // Translate so the model base (yMin) sits at ground level, not centroid
            glm::vec3 c = pm.centroid();
            M = glm::translate(M, {-c.x, -pm.yMin(), -c.z});
            glm::mat3 N = glm::mat3(glm::transpose(glm::inverse(M)));

            bool sway = isVegetation(p.model) || p.model == "tree_of_life";
            toon.setBool("uSway", sway);

            if (p.model == "tree_of_life" && pm.hasVertexColor()) {
                // Texture colors baked into vertices — ambient ramps up while charging,
                // then fades out after release (treeGlow) rather than snapping off
                float pulse = 0.22f + 0.12f * std::sin(elapsed * 0.7f)
                            + treeGlow * 0.40f;
                toon.use();
                toon.setFloat("uAmbientStrength",  pulse);
                toon.setBool ("uUseVertexColor",   true);
                toon.setBool ("uMultiColor",        false);
                toon.setBool ("uCrystal",           false);
                toon.setBool ("uWave",              false);
                toon.setFloat("uAlpha",             1.0f);
                toon.setBool ("uCircleClip",        false);
                toon.setBool ("uShoreBlend",        false);
                toon.setMat4 ("uModel",     glm::value_ptr(M));
                toon.setMat3 ("uNormalMat", glm::value_ptr(N));
                toon.setBool ("uEmissive",  false);
                toon.setFloat("uTime",      elapsed);
                toon.setBool ("uSway",      true);
                pm.draw();
                toon.setBool ("uUseVertexColor",   false);
                toon.setFloat("uAmbientStrength",  0.22f * ambScale);
                toon.setBool ("uSway", false);
            } else {
                drawMesh(toon, outline, pm, M, N, colorFor(p.model), view, proj, elapsed, sway);
            }
        }
    }
    toon.setBool("uSway", false);
}

void drawResources(Shader& egg, const std::unique_ptr<TexturedMesh>* gemTex,
                   const MapData& map, const glm::vec3& sunDir,
                   const glm::vec3& lightCol, float ambScale,
                   const glm::mat4& view, const glm::mat4& proj, float elapsed)
{
    bool anyGem = false;
    for (int i = 1; i < 7; ++i) anyGem |= (bool)gemTex[i];
    if (!anyGem) return;

    egg.use();
    egg.setMat4 ("uView", glm::value_ptr(view));
    egg.setMat4 ("uProj", glm::value_ptr(proj));
    egg.setInt  ("uTex", 0);
    egg.setVec3 ("uDirLightDir",   sunDir.x, sunDir.y, sunDir.z);
    egg.setVec3 ("uDirLightColor", lightCol.x, lightCol.y, lightCol.z);
    egg.setVec3 ("uPointPos",   0.f, 10.f, 0.f);
    egg.setVec3 ("uPointColor", 0.f, 0.f, 0.f);
    egg.setFloat("uAmbient", 0.55f * ambScale);
    egg.setFloat("uAlpha",   1.f);
    egg.setFloat("uBright",  1.15f);
    glDisable(GL_CULL_FACE);

    const float spreadR = 0.30f;
    for (auto& tile : map.tiles) {
        bool anyRes = false;
        for (int i = 1; i < 7; ++i) anyRes |= (tile.res[i] > 0 && gemTex[i]);
        if (!anyRes) continue;

        float wx = tile.x * TILE + TILE * 0.5f;
        float wz = tile.y * TILE + TILE * 0.5f;
        float tileHash = float(tile.x * 17 + tile.y * 31);

        int slot = 0;
        for (int i = 1; i < 7; ++i) {           // stones only (food drawn below)
            if (tile.res[i] == 0 || !gemTex[i]) continue;
            TexturedMesh* gm = gemTex[i].get();

            float angle = slot * (2.f * 3.14159f / 7.f) + tileHash * 0.05f;
            float ox = spreadR * std::sin(angle);
            float oz = spreadR * std::cos(angle);
            float bob  = 0.35f + 0.08f * std::sin(elapsed * 1.5f + tileHash * 0.12f + i * 0.9f);
            float spin = elapsed * 1.2f + tileHash * 0.08f + i * 0.45f;
            float r = gm->boundingRadius();
            float s = (r > 1e-4f) ? 0.22f / r : 0.22f;

            glm::mat4 M = glm::translate(glm::mat4(1.f), {wx+ox, bob, wz+oz});
            M = glm::rotate(M, spin, {0.f, 1.f, 0.f});
            M = glm::scale(M, glm::vec3(s));
            M = glm::translate(M, -gm->centroid());
            glm::mat3 N3 = glm::mat3(glm::transpose(glm::inverse(M)));

            egg.setMat4("uModel",     glm::value_ptr(M));
            egg.setMat3("uNormalMat", glm::value_ptr(N3));
            gm->draw();
            ++slot;
        }
    }
    glEnable(GL_CULL_FACE);
}

void drawFood(Shader& egg, const std::unique_ptr<TexturedMesh>* foodMesh,
              const MapData& map, const glm::vec3& sunDir,
