#include "app/SkyDayNight.hpp"
#include <cstdlib>
#include <cmath>

DayNight computeDayNight(float elapsed, int nightOverride)
{
    const float DAY_LENGTH = std::getenv("ZAPPY_DAY_LENGTH")
                           ? (float)std::atof(std::getenv("ZAPPY_DAY_LENGTH")) : 120.f;
    const float DAY_START  = std::getenv("ZAPPY_DAY_START")
