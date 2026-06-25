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
