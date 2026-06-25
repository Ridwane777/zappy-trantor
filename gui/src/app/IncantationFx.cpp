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
