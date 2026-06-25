#include "render/Camera.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

static constexpr float PI = 3.14159265358979323846f;

glm::vec3 Camera::getPos() const {
    float cosP = std::cos(pitch);
    float sinP = std::sin(pitch);
    float cosY = std::cos(yaw);
    float sinY = std::sin(yaw);
    return target + glm::vec3(
        distance * cosP * sinY,
        distance * sinP,
        distance * cosP * cosY
    );
}

