#pragma once
#include <glm/glm.hpp>

// Result of the client-side day/night clock: the sky colours, the light
// direction/colour and ambient scale that the whole scene reads from.
struct DayNight {
    float     nightF;     // 0 = full day, 1 = night
    float     ambScale;   // ambient multiplier (moonlit floor at night)
    glm::vec3 skySunDir;  // fixed warm sun direction (for the sky disc)
    glm::vec3 moonDir;    // moon direction (low, opposite the sun)
    glm::vec3 sunDir;     // scene light direction (sun by day, moon by night)
    glm::vec3 lightCol;   // scene light colour (warm day -> cool moonlight)
    glm::vec3 skyTop;     // sky gradient top colour
    glm::vec3 skyHor;     // sky gradient horizon colour
