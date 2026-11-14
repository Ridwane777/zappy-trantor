#include "app/GuiConfig.hpp"
#include "world/World.hpp"

#include <algorithm>
#include <cstdlib>

namespace {

const char *env(const char *name) { return std::getenv(name); }

float envFloat(const char *name, float def)
{
    const char *v = env(name);
    return v ? (float)std::atof(v) : def;
}

int envInt(const char *name, int def)
{
    const char *v = env(name);
    return v ? std::atoi(v) : def;
}

std::string envString(const char *name, const std::string &def)
{
    const char *v = env(name);
    return v ? std::string(v) : def;
}

}

GuiConfig::GuiConfig() : strideDist(TILE) {}

GuiConfig GuiConfig::fromEnvironment()
{
    GuiConfig c;
    c.cdnUrl = envString("ZAPPY_CDN_URL", c.cdnUrl);
    if (const char *f = env("ZAPPY_FORCE_CDN"); f && *f && *f != '0')
        c.forceCdn = true;

    c.autostart = envString("ZAPPY_AUTOSTART", "");
    c.debug     = env("ZAPPY_DEBUG") != nullptr;
    c.testMode  = env("ZAPPY_TESTPLAYERS") != nullptr;
    if (c.testMode) {
        c.testPlayerCount = envInt("ZAPPY_TESTPLAYERS", 4);
        if (c.testPlayerCount <= 0) c.testPlayerCount = 4;
    }
    c.testPhaseSec = envFloat("ZAPPY_TESTSEC", c.testPhaseSec);

    c.playerScale = envFloat("ZAPPY_PSCALE", c.playerScale);
    if (env("ZAPPY_STRIDE"))
        c.strideDist = std::max(0.2f, envFloat("ZAPPY_STRIDE", c.strideDist));

    c.dayLength = envFloat("ZAPPY_DAY_LENGTH", c.dayLength);
    c.dayStart  = envFloat("ZAPPY_DAY_START",  c.dayStart);
    c.constAz   = envFloat("ZAPPY_CONST_AZ",   c.constAz);
    c.constEl   = envFloat("ZAPPY_CONST_EL",   c.constEl);
    c.constS    = envFloat("ZAPPY_CONST_S",    c.constS);

    c.phoenixYaw = envFloat("ZAPPY_PHX_YAW", c.phoenixYaw);

    c.mewFloor       = envFloat("ZAPPY_MEW_FLOOR",        c.mewFloor);
    c.mewScale       = envFloat("ZAPPY_MEW_SCALE",        c.mewScale);
    c.mewPose        = envFloat("ZAPPY_MEW_POSE",         c.mewPose);
    c.charzScale     = envFloat("ZAPPY_CHARZ_SCALE",      c.charzScale);
    c.charzGroundY   = envFloat("ZAPPY_CHARZ_GROUND_Y",   c.charzGroundY);
    c.charzIdle      = envInt  ("ZAPPY_CHARZ_IDLE",       c.charzIdle);
    c.charzFlyIdle   = envInt  ("ZAPPY_CHARZ_FLY_IDLE",   c.charzFlyIdle);
    c.charzAnimSpeed = envFloat("ZAPPY_CHARZ_ANIM_SPEED", c.charzAnimSpeed);
    return c;
}
