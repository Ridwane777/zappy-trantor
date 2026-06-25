#pragma once
#include <glm/glm.hpp>

class Camera {
public:
    glm::vec3 target   = {0.f, 0.f, 0.f};
    float     distance = 10.f;
    float     yaw      = 0.f;    // horizontal rotation (radians)
