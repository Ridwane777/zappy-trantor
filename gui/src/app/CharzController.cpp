#include "app/CharzController.hpp"
#include "app/GuiConfig.hpp"
#include "audio/AudioManager.hpp"
#include "render/SkinnedMesh.hpp"

#include <SFML/Window/Keyboard.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace {
float animDurOr(const SkinnedMesh &m, bool have, int idx, float def)
{
    return (have && m.animCount() > idx) ? m.animDuration(idx) : def;
}
}

CharzController::CharzController(const GuiConfig &cfg, const SkinnedMesh &mesh,
                                 bool haveMesh, float cx, float cz)
    : charzPos(cx, cfg.charzGroundY, cz),
      charzAnimIdx(cfg.charzIdle),
      charzMesh(mesh), haveCharz(haveMesh),
      CHARZ_GROUND_Y(cfg.charzGroundY),
      CA_IDLE(cfg.charzIdle), CA_FLY_IDLE(cfg.charzFlyIdle),
      CHARZ_ANIM_SPEED(cfg.charzAnimSpeed),
      CHARZ_FLY_Y(cfg.charzGroundY + 6.f),
      caTakeoffDur(animDurOr(mesh, haveMesh, CA_TAKEOFF, 0.8f)),
      caLandADur(animDurOr(mesh, haveMesh, CA_LAND_A, 0.5f)),
      caLandBDur(animDurOr(mesh, haveMesh, CA_LAND_B, 0.7f))
{
}

void CharzController::enterFlying(float cx, float cz, float camYaw)
{
    charzMove = CharzMove::FLYING; charzMoveT = 0.f;
    charzPos  = glm::vec3(
        cx + std::sin(camYaw) * 12.f,
        12.f,
        cz + std::cos(camYaw) * 12.f);
    charzYaw    = camYaw + glm::radians(180.f);
    charzAnimTime = 0.f; charzAnimIdx = CA_IDLE;
    charzBobAmp   = 0.08f;
}

void CharzController::update(float dt, AudioManager &gameAudio)
{
    const float turn = 2.0f;
    glm::vec3 fwd(std::sin(charzYaw), 0.f, std::cos(charzYaw));
    bool fwd_k = sf::Keyboard::isKeyPressed(sf::Keyboard::Up)   || sf::Keyboard::isKeyPressed(sf::Keyboard::W);
    bool bck_k = sf::Keyboard::isKeyPressed(sf::Keyboard::Down);
    bool run_k = sf::Keyboard::isKeyPressed(sf::Keyboard::Space);
    bool up_k  = sf::Keyboard::isKeyPressed(sf::Keyboard::Z);
    bool dn_k  = sf::Keyboard::isKeyPressed(sf::Keyboard::S);
    bool moving = fwd_k || bck_k;

    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left))  charzYaw += turn * dt;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right)) charzYaw -= turn * dt;

    bool lft_k = sf::Keyboard::isKeyPressed(sf::Keyboard::Left);
    bool rgt_k = sf::Keyboard::isKeyPressed(sf::Keyboard::Right);

    switch (charzMove) {
    case CharzMove::GROUNDED: {
        float spd = run_k ? 18.f : 6.f;
        if (fwd_k) charzPos += fwd * spd * dt;
        if (bck_k) charzPos -= fwd * spd * dt;
        charzPos.y = CHARZ_GROUND_Y;
        if      (moving && run_k) { charzAnimIdx = CA_RUN;    charzBobAmp = 0.02f; }
        else if (moving)          { charzAnimIdx = CA_WALK;   charzBobAmp = 0.03f; }
        else if (lft_k)           { charzAnimIdx = CA_TURN_L; charzBobAmp = 0.01f; }
        else if (rgt_k)           { charzAnimIdx = CA_TURN_R; charzBobAmp = 0.01f; }
        else                      { charzAnimIdx = CA_IDLE;   charzBobAmp = 0.04f; }
        if (up_k) {
            charzMove = CharzMove::TAKEOFF; charzMoveT = 0.f;
            charzAnimIdx = CA_TAKEOFF; charzAnimTime = 0.f; charzBobAmp = 0.f;
        }
        break;
    }
    case CharzMove::TAKEOFF: {
        charzMoveT += dt;
        charzBobAmp = 0.f;
        if (charzMoveT < caTakeoffDur) {

            charzPos.y    = CHARZ_GROUND_Y;
            charzAnimIdx  = CA_TAKEOFF;
            charzAnimTime = std::min(charzMoveT, caTakeoffDur - 0.01f);
        } else {

            float k = glm::clamp((charzMoveT - caTakeoffDur) / caClimbDur,
                                 0.f, 1.f);
            k = k * k * (3.f - 2.f * k);
            charzPos.y    = CHARZ_GROUND_Y + (CHARZ_FLY_Y - CHARZ_GROUND_Y) * k;
            charzAnimIdx  = CA_FLY;
            charzAnimTime = charzMoveT - caTakeoffDur;
        }
        if (charzMoveT >= caTakeoffDur + caClimbDur) {
            charzMove = CharzMove::FLYING; charzAnimTime = 0.f;
            gameAudio.playLoop("wings_flapping", 70.f);
        }
        break;
    }
    case CharzMove::FLYING: {
        float spd = run_k ? 30.f : 18.f, vspd = 14.f;
        if (fwd_k) charzPos += fwd * spd * dt;
        if (bck_k) charzPos -= fwd * spd * dt;
        if (up_k)  charzPos.y += vspd * dt;
        if (dn_k)  charzPos.y -= vspd * dt;
        if      (moving && run_k) { charzAnimIdx = CA_AIR_RUN;   charzBobAmp = 0.04f; }
        else if (moving)          { charzAnimIdx = CA_AIR_WALK;  charzBobAmp = 0.06f; }
        else if (dn_k)            { charzAnimIdx = CA_FALL;      charzBobAmp = 0.04f; }
        else if (up_k)            { charzAnimIdx = CA_FLY;       charzBobAmp = 0.04f; }
        else if (lft_k)           { charzAnimIdx = CA_AIR_TURN_L;charzBobAmp = 0.04f; }
        else if (rgt_k)           { charzAnimIdx = CA_AIR_TURN_R;charzBobAmp = 0.04f; }
        else                      { charzAnimIdx = CA_FLY_IDLE;  charzBobAmp = 0.08f; }
        if (charzPos.y <= CHARZ_GROUND_Y) {
            charzPos.y = CHARZ_GROUND_Y;
            charzMove = CharzMove::LANDING_A; charzMoveT = 0.f;
            charzAnimIdx = CA_LAND_A; charzAnimTime = 0.f; charzBobAmp = 0.f;
            gameAudio.stopLoop();
        }
        break;
    }
    case CharzMove::LANDING_A: {
        charzMoveT += dt;
        charzPos.y = CHARZ_GROUND_Y;
        charzAnimIdx = CA_LAND_A;
        charzAnimTime = std::min(charzMoveT, caLandADur - 0.01f);
        charzBobAmp = 0.f;
        if (charzMoveT >= caLandADur) {
            charzMove = CharzMove::LANDING_B; charzMoveT = 0.f;
            charzAnimIdx = CA_LAND_B; charzAnimTime = 0.f;
        }
        break;
    }
    case CharzMove::LANDING_B: {
        charzMoveT += dt;
        charzPos.y = CHARZ_GROUND_Y;
        charzAnimIdx = CA_LAND_B;
        charzAnimTime = std::min(charzMoveT, caLandBDur - 0.01f);
        charzBobAmp = 0.f;
        if (charzMoveT >= caLandBDur) {
            charzMove = CharzMove::GROUNDED; charzAnimTime = 0.f;
        }
        break;
    }
    }

    if (charzMove == CharzMove::GROUNDED || charzMove == CharzMove::FLYING) {

        if (charzAnimIdx != charzPrevAnim) {
            charzAnimTime = 0.f;
            charzPrevAnim = charzAnimIdx;
        }
        charzAnimTime += dt * CHARZ_ANIM_SPEED;

        if (haveCharz && charzMesh.animCount() > charzAnimIdx) {
            float dur = charzMesh.animDuration(charzAnimIdx);
            if (dur > 0.01f && charzAnimTime >= dur)
                charzAnimTime = std::fmod(charzAnimTime, dur);
        }
    }
}
