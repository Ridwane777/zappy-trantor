#include "app/SkyDayNight.hpp"
#include <cstdlib>
#include <cmath>

DayNight computeDayNight(float elapsed, int nightOverride)
{
    const float DAY_LENGTH = std::getenv("ZAPPY_DAY_LENGTH")
                           ? (float)std::atof(std::getenv("ZAPPY_DAY_LENGTH")) : 120.f;
    const float DAY_START  = std::getenv("ZAPPY_DAY_START")
                           ? (float)std::atof(std::getenv("ZAPPY_DAY_START")) : 0.05f;
    float dayPhase = std::fmod(elapsed / DAY_LENGTH + DAY_START, 1.f);

    // nightF: 0 = full day (identical to the original look), 1 = night.
    // Decoupled from any moving sun — just a clock with smooth dusk/dawn.
    float nightF = glm::clamp(glm::smoothstep(0.45f, 0.55f, dayPhase)
                            - glm::smoothstep(0.95f, 1.00f, dayPhase), 0.f, 1.f);
    if      (nightOverride == 1) nightF = 1.f;   // N: force night
    else if (nightOverride == 2) nightF = 0.f;   // N: force day
