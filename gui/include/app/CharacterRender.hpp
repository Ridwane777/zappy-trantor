#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "app/Character.hpp"

class Shader;

// Updates + draws the standalone island characters: their idle/lie-down state
// machine, the solid skinned pass, and the additive green full-body aura that
// fades in/out while their auraTimer (set by the Tree of Life wave) runs.
void drawCharacters(std::vector<Character>& chars, Shader& skin, Shader& aura,
