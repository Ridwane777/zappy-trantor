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
    skin.setBool ("uMultiColor",      false);

    for (auto& ch : chars) {
        if (!ch.incanting) ch.animTime += dt;
        ch.stateTimer += dt;

        int    drawAnim = ch.idleAnim;
        bool   loop     = true;
        bool   rev      = false;

        if (ch.fallAnim >= 0) {
            float fallDur = ch.mesh.animDuration(ch.fallAnim);

            switch (ch.state) {
            case CharState::IDLE:
                drawAnim = ch.idleAnim; loop = true; rev = false;
                if (ch.stateTimer >= 10.f) {
                    ch.state = CharState::FALLING;
                    ch.animTime = 0.f;
                }
                break;

            case CharState::FALLING:
                drawAnim = ch.fallAnim; loop = false; rev = false;
                if (ch.animTime >= fallDur) {
                    ch.state = CharState::GROUNDED;
                    ch.stateTimer = 0.f;
                }
                break;

            case CharState::GROUNDED:
                // hold last frame
                drawAnim = ch.fallAnim; loop = false; rev = false;
                ch.animTime = fallDur;
                if (ch.stateTimer >= 30.f) {
                    ch.state = CharState::RISING;
                    ch.animTime = 0.f;
                }
                break;

