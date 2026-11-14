#pragma once

#include <string>

// Every tunable of the GUI read from the environment, gathered in one place.
// The environment is read once at startup by fromEnvironment(); subsystems get
// this struct (or the fields they need) instead of calling getenv themselves.
struct GuiConfig {
    // Assets
    std::string cdnUrl   = "https://pub-6d9e00b90894418295af58a44ecfe1df.r2.dev"; // ZAPPY_CDN_URL
    bool        forceCdn = false;  // ZAPPY_FORCE_CDN (set and not "0")

    // Startup / debug
    std::string autostart;         // ZAPPY_AUTOSTART "host[:port]", empty = show menu
    bool        debug = false;     // ZAPPY_DEBUG (presence)
    bool        testMode = false;  // ZAPPY_TESTPLAYERS (presence)
    int         testPlayerCount = 4;  // ZAPPY_TESTPLAYERS value, <= 0 falls back to 4
    float       testPhaseSec = 1.f;   // ZAPPY_TESTSEC

    // Players
    float playerScale = 0.05f / 6.f;  // ZAPPY_PSCALE
    float strideDist;                 // ZAPPY_STRIDE (min 0.2), default TILE

    // Sky
    float dayLength = 120.f;    // ZAPPY_DAY_LENGTH
    float dayStart  = 0.05f;    // ZAPPY_DAY_START
    float constAz   = 225.f;    // ZAPPY_CONST_AZ (degrees)
    float constEl   = 22.f;     // ZAPPY_CONST_EL (degrees)
    float constS    = 0.040f;   // ZAPPY_CONST_S

    // Menu
    float phoenixYaw = 0.f;     // ZAPPY_PHX_YAW (degrees)

    // Easter-egg mounts
    float mewFloor = 1.0f;      // ZAPPY_MEW_FLOOR
    float mewScale = 0.27f;     // ZAPPY_MEW_SCALE
    float mewPose  = 2.0f;      // ZAPPY_MEW_POSE
    float charzScale     = 0.9f;  // ZAPPY_CHARZ_SCALE
    float charzGroundY   = 0.0f;  // ZAPPY_CHARZ_GROUND_Y
    int   charzIdle      = 1;     // ZAPPY_CHARZ_IDLE
    int   charzFlyIdle   = 49;    // ZAPPY_CHARZ_FLY_IDLE
    float charzAnimSpeed = 1.5f;  // ZAPPY_CHARZ_ANIM_SPEED

    GuiConfig();
    static GuiConfig fromEnvironment();
};
