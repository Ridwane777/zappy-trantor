#include "app/IncantationFx.hpp"
#include "render/Shader.hpp"
#include "render/Mesh.hpp"
#include "world/World.hpp"        // TILE, buildDashedRingMesh, buildVQuadMesh

#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>

void drawIncantations(const std::vector<IncantationEffect>& fx, Shader& incant,
                      const glm::mat4& view, const glm::mat4& proj,
                      const glm::vec3& camPos, float elapsed)
{
    if (fx.empty()) return;
    static Mesh dashedRing = buildDashedRingMesh(8, 0.55f);
    static Mesh vquad      = buildVQuadMesh();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);

    incant.use();
    incant.setMat4("uView",    glm::value_ptr(view));
    incant.setMat4("uProj",    glm::value_ptr(proj));
    incant.setVec3("uCamPos",  camPos.x, camPos.y, camPos.z);

    const float DOMAIN_R  = TILE * 0.58f;
    const float INNER_R   = DOMAIN_R * 0.84f;
    const int   N_PILLARS = 8;
    const float PI2       = 6.28318f;

    for (auto& ix : fx) {
        float fadeIn  = glm::clamp(ix.timer / 0.25f, 0.f, 1.f);
        float fadeOut = glm::clamp((INCANT_DUR - ix.timer) / 0.4f, 0.f, 1.f);
        float fade    = fadeIn * fadeOut;
        float ringRot = elapsed * 0.20f;

        // ── dashed ring with Fresnel + radial glow ────────────────
        incant.setBool("uPillarMode", false);
        incant.setVec3("uRingCenter", ix.pos.x, ix.pos.y, ix.pos.z);
        incant.setFloat("uRingInnerR", INNER_R);
