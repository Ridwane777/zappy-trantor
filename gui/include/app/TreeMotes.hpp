#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "app/Particle.hpp"

class Shader;

// The Tree of Life's energy motes: sickly green particles that gather up the
// trunk while the pulse charges, then burst outward in a ring on release.
// GuiApp owns the pulse state machine and passes its phase in as flags.
//
//   charging     : true while the pulse is charging (spawns gathering motes)
//   chargeT      : 0..1 charge progress (colour + spawn rate)
//   justReleased : true on the single frame the pulse fires (spawns the ring)
//
