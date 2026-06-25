#pragma once
#include <glm/glm.hpp>

// Result of the client-side day/night clock: the sky colours, the light
// direction/colour and ambient scale that the whole scene reads from.
struct DayNight {
    float     nightF;     // 0 = full day, 1 = night
    float     ambScale;   // ambient multiplier (moonlit floor at night)
    glm::vec3 skySunDir;  // fixed warm sun direction (for the sky disc)
