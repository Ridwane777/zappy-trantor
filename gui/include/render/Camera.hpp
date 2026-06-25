#pragma once
#include <glm/glm.hpp>

class Camera {
public:
    glm::vec3 target   = {0.f, 0.f, 0.f};
    float     distance = 10.f;
    float     yaw      = 0.f;    // horizontal rotation (radians)
    float     pitch    = 0.5f;   // vertical angle (radians)

    glm::mat4 getView()             const;
    glm::mat4 getProjection(float aspect) const;
