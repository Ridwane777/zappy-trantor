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
