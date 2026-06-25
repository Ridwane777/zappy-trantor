#include "app/CharacterRender.hpp"
#include "render/Shader.hpp"

#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>

void drawCharacters(std::vector<Character>& chars, Shader& skin, Shader& aura,
                    const glm::mat4& view, const glm::mat4& proj,
                    const glm::vec3& camPos, const glm::vec3& sunDir,
                    const glm::vec3& lightCol, float ambScale,
                    float elapsed, float dt)
{
    if (chars.empty()) return;
    glm::mat4 boneMats[MAX_BONES];
    skin.use();
    skin.setMat4("uView", glm::value_ptr(view));
    skin.setMat4("uProj", glm::value_ptr(proj));
    skin.setVec3("uCamPos",          camPos.x, camPos.y, camPos.z);
    skin.setVec3("uDirLightDir",     sunDir.x, sunDir.y, sunDir.z);
    skin.setVec3("uDirLightColor",   lightCol.x, lightCol.y, lightCol.z);
    skin.setVec3("uPointLight0Color",0.f,0.f,0.f);
    skin.setVec3("uPointLight1Color",0.f,0.f,0.f);
    skin.setFloat("uShininess",      32.f);
    skin.setFloat("uAmbientStrength",0.22f * ambScale);
    skin.setFloat("uAlpha",          1.0f);
    skin.setFloat("uTime",           elapsed);
    skin.setBool ("uEmissive",       false);
    skin.setBool ("uCrystal",        false);
    skin.setBool ("uWave",           false);
    skin.setBool ("uSway",           false);
    skin.setBool ("uCircleClip",      false);
    skin.setBool ("uShoreBlend",      false);
    skin.setFloat("uShoreInnerR",     0.f);
    skin.setFloat("uShoreWidth",      1.f);
    skin.setBool ("uUseTexture",      true);
    skin.setInt  ("uTexture",         0);
    skin.setBool ("uUseVertexColor",  false);
