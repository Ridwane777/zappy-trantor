#include "app/Constellation.hpp"
#include "render/Shader.hpp"
#include "world/World.hpp"        // frnd(a, b)

#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>

// links between the 7 Big Dipper stars (the bowl + the handle)
static const int kDipperLink[7][2] = { {0,1},{1,2},{2,3},{3,0},{3,4},{4,5},{5,6} };

Constellation::Constellation()
{
    // scattered background stars (little intro-style lights)
    auto rnd = [](float a, float b) { return a + (b - a) * (float)std::rand() / RAND_MAX; };
    for (int i = 0; i < 170; ++i) {
        float az = rnd(0.f, 6.2831853f);
        float el = rnd(0.06f, 1.35f);               // elevation above horizon (rad)
        float ce = std::cos(el), se = std::sin(el);
        SkyStar s;
        s.dir    = glm::normalize(glm::vec3(ce * std::cos(az), se, ce * std::sin(az)));
        s.size   = rnd(1.6f, 4.2f);
        s.phase  = rnd(0.f, 6.28f);
        s.twFreq = rnd(1.5f, 6.0f);
        s.bright = rnd(0.35f, 0.9f);
        _stars.push_back(s);
    }
    // Big Dipper, placed among the far stars at a tunable sky direction
    // (azimuth/elevation in degrees) so it sits in the visible sky behind the tree.
    float constAz = std::getenv("ZAPPY_CONST_AZ") ? (float)std::atof(std::getenv("ZAPPY_CONST_AZ")) : 225.f;
    float constEl = std::getenv("ZAPPY_CONST_EL") ? (float)std::atof(std::getenv("ZAPPY_CONST_EL")) : 22.f;
    float constS  = std::getenv("ZAPPY_CONST_S")  ? (float)std::atof(std::getenv("ZAPPY_CONST_S"))  : 0.040f;
    float az = constAz * 3.14159265f / 180.f, el = constEl * 3.14159265f / 180.f;
    float ce = std::cos(el), se = std::sin(el);
    glm::vec3 cdir = glm::normalize(glm::vec3(ce * std::cos(az), se, ce * std::sin(az)));
    _dipperCenter = cdir;
    glm::vec3 right = glm::normalize(glm::cross(glm::vec3(0,1,0), cdir));
    glm::vec3 upv   = glm::normalize(glm::cross(cdir, right));
    glm::vec2 P[7] = { {0.0f,3.0f}, {0.0f,0.0f}, {3.0f,0.2f}, {3.2f,2.7f},
                       {5.0f,3.3f}, {6.8f,3.7f}, {8.6f,4.6f} };
    glm::vec2 ctr(3.7f, 2.6f);
    for (int i = 0; i < 7; ++i) {
        glm::vec2 q = (P[i] - ctr) * constS;
        _dipperDir[i] = glm::normalize(cdir + right * q.x + upv * q.y);
    }
}

void Constellation::tick(bool spaceHeld, bool adminMode, float dt)
{
    if (adminMode) {
        if (spaceHeld && !_active) {
            _charge += dt;
            if (_charge >= 5.f) {                  // trigger
                _active = true; _waveT = 0.f; _litT = 0.f;
            }
        } else if (!_active) {
            _charge = std::max(0.f, _charge - dt * 2.f);   // release -> fade
        }
    }
    if (_waveT >= 0.f) _waveT += dt;                       // run the wave
    if (_active) {                                         // 5 s illumination
        _litT += dt;
        if (_litT >= 5.f) {                               // done -> reset
            _active = false; _litT = -1.f; _charge = 0.f;
        }
    }
}

void Constellation::updateMewEnergy(bool adminMode, float dt, const glm::vec3& mewPos, float elapsed)
{
    glm::vec3 mc(mewPos.x, mewPos.y + std::sin(elapsed * 2.5f) * 0.4f, mewPos.z);
    glm::vec3 pa(0.78f, 0.58f, 1.0f);   // light purple
    glm::vec3 pb(0.55f, 0.30f, 0.96f);  // deeper violet

    // charging: motes gather and spiral up around Mew
    if (adminMode && _charge > 0.f && !_active) {
        float cf = glm::clamp(_charge / 5.f, 0.f, 1.f);
        int n = (int)(cf * cf * 6.f) + (frnd(0.f, 1.f) < cf ? 1 : 0);
        for (int i = 0; i < n; ++i) {
            Spark m;
            float ang = frnd(0.f, 6.2832f), rad = frnd(0.5f, 1.7f);
            m.pos = mc + glm::vec3(std::cos(ang) * rad, frnd(-0.6f, 1.0f), std::sin(ang) * rad);
            glm::vec3 in = glm::normalize(glm::vec3(mc.x - m.pos.x, 0.f, mc.z - m.pos.z) + glm::vec3(1e-4f));
            m.vel = glm::vec3(in.x * frnd(0.4f, 1.3f), frnd(1.6f, 3.6f), in.z * frnd(0.4f, 1.3f));
            m.maxLife = frnd(0.8f, 1.6f); m.life = m.maxLife;
            m.size = frnd(0.10f, 0.24f);
            m.phase = frnd(0.f, 6.28f); m.twFreq = frnd(16.f, 34.f);
            m.color = glm::mix(pa, pb, frnd(0.f, 1.f)) * frnd(0.85f, 1.2f);
            _motes.push_back(m);
        }
    }
    // activation: a dense ring/dome of purple energy bursts outward from Mew
    if (_waveT >= 0.f && _wavePrev < 0.f) {
        const int n = 240;
        for (int i = 0; i < n; ++i) {
            Spark m;
            float ang = (i / (float)n) * 6.2832f + frnd(-0.04f, 0.04f);
            float elev = frnd(-0.2f, 0.9f);                 // mostly outward, some upward (dome)
            float ce = std::cos(elev), se = std::sin(elev);
            m.pos = mc + glm::vec3(std::cos(ang) * 0.4f, 0.3f, std::sin(ang) * 0.4f);
            float spd = frnd(10.f, 20.f);
            m.vel = glm::vec3(std::cos(ang) * ce * spd, se * spd, std::sin(ang) * ce * spd);
