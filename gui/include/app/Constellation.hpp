#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "app/Particle.hpp"

class Shader;

// The night-sky feature: scattered background stars + the Big Dipper, plus the
// admin "constellation channel" — hold Space near Mew to charge a purple-energy
// aura that, after 5 s, releases a wave and lights the constellation for 5 s.
//
// State (charge / activation / illumination timers + the energy motes) lives
// here; GuiApp drives it each frame via tick()/updateMewEnergy() and draws it
// with renderNightSky()/renderMewEnergy(). Placement is env-tunable
// (ZAPPY_CONST_AZ / _EL / _S).
class Constellation {
public:
    Constellation();   // generates the star field + Big Dipper directions

    // Charge/illumination timers. spaceHeld charges while in adminMode; releasing
    // fades it; reaching 5 s triggers the wave + a 5 s lit envelope.
    void tick(bool spaceHeld, bool adminMode, float dt);

    // Spawn + integrate the purple energy motes around Mew (gather while charging,
    // a dome burst on activation). mewPos is Mew's current world position.
