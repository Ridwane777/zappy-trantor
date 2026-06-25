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

    // DAY stays exactly as before: low warm sun + orange horizon. The sun disc
    // simply fades out at night; the moon (low, toward the default view) fades in.
    glm::vec3 skySunDir = glm::normalize(glm::vec3(0.680f, 0.08f, 0.680f));
    glm::vec3 moonDir   = glm::normalize(glm::vec3(-0.593f, 0.545f, -0.593f));
    glm::vec3 sunDir    = glm::normalize(glm::mix(skySunDir, moonDir, nightF));
    glm::vec3 lightCol  = glm::mix(glm::vec3(1.0f, 0.80f, 0.55f),          // warm day
                                   glm::vec3(0.55f, 0.66f, 0.98f), nightF); // moonlight
    float ambScale = 1.f - 0.28f * nightF;                                 // moonlit floor
