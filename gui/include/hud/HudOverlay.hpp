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
    void init(const sf::Font &font) { _font = &font; }

    void toggle() { _open = !_open; }
    void update(float dt) {
        _anim += ((_open ? 1.f : 0.f) - _anim) * std::min(1.f, dt * 10.f);
    }

    // Hit-test a left-click against last frame's UI. Returns true if the UI
    // consumed it (so the caller should not start a camera drag).
    bool handleClick(sf::Vector2i mouse);

    // Scroll the team/player list when the wheel is over the open panel.
    // Returns true if consumed (so the caller should not zoom the camera).
    bool handleScroll(sf::Vector2i mouse, float wheelDelta);

    // While a player popup is open, returns that player's id roughly every
