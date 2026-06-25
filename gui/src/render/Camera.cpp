#include "render/Camera.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

static constexpr float PI = 3.14159265358979323846f;

glm::vec3 Camera::getPos() const {
    float cosP = std::cos(pitch);
