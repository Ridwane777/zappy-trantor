#pragma once
#include <glm/glm.hpp>

// A single additive billboard particle, shared by the evolution sparkles, the
// tree-of-life motes and Mew's purple-energy channel. Drawn with the `star`
// shader as a GL_TRIANGLE_FAN quad facing the camera.
struct Spark {
    glm::vec3 pos, vel;
