#pragma once
// 2D overlay for the game view: the top HUD status line, a retractable
// team/player sidebar (with per-player inventory popup and a resource-reset
// countdown), and the bottom-left action feed. Owns the sidebar selection state
// and the 2D click hit-testing.

#include <SFML/Graphics.hpp>
#include <glm/glm.hpp>
#include <algorithm>
#include <string>
#include <vector>

#include "net/GuiState.hpp"

class HudOverlay {
public:
