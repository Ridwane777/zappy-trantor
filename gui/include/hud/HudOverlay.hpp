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
    // 0.33s (else -1) so the caller can request its inventory ("pin #id").
    int inventoryPollTarget(float dt);

    // Draw the whole overlay; rebuilds the hit regions for the next frame.
    // `state` is null when offline. `mapW/mapH` feed the offline status line.
    void draw(sf::RenderWindow &win, const GuiState *state,
              const std::string &host, int port, int mapW, int mapH);

    const std::string &selectedTeam() const { return _selTeam; }

    // Distinct, readable per-team colour (indexed by team order).
    static glm::vec3 teamColor3(int i);

private:
    // kind: 0=team header, 1=player row, 2=toggle tab, 3=panel background
    struct Hit { sf::FloatRect rect; int kind; std::string team; int pid; };

    static sf::Color teamColorU(int i);

    const sf::Font   *_font      = nullptr;
    bool              _open      = true;
    float             _anim      = 1.f;   // 0=hidden, 1=shown (slide)
    std::string       _selTeam;           // "" = none → no team glow
    int               _selPlayer = -1;    // -1 = none → no inventory popup
    float             _invPollT  = 0.f;
    std::vector<Hit>  _hits;
    float             _scroll    = 0.f;   // team/player list scroll offset (px)
    float             _maxScroll = 0.f;   // set during draw, clamps _scroll
    float             _panelX    = 0.f;   // set during draw, for scroll hit-test

    static constexpr float WIDTH = 250.f;
};
