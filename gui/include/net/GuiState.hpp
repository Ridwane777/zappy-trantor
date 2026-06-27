#pragma once
// Authoritative game state as reported by the server over the GRAPHIC protocol.
// The network client mutates this; the renderer reads it once per frame.

#include <array>
#include <string>
#include <vector>
#include <deque>
#include <unordered_map>

// Resource indices match the server: 0=food 1=linemate 2=deraumere 3=sibur
// 4=mendiane 5=phiras 6=thystame.
inline constexpr int GUI_RES_COUNT = 7;

struct GuiTile {
    std::array<int, GUI_RES_COUNT> res{};   // quantities on this tile
};

// One line in the on-screen action feed. The renderer colors the name by team.
struct GuiLogEntry {
    int         playerId = 0;
    std::string team;            // team name → name color
    std::string action;          // e.g. "grabbed food", "reached level 3", "died"
    int         kind = 0;        // 0=grab 1=levelup 2=death (action-text tint)
};

struct GuiPlayer {
    int         id          = 0;
    int         x           = 0;
    int         y           = 0;
    int         orientation = 1;   // 1=N 2=E 3=S 4=W (server convention)
    int         level       = 1;
    std::string team;
    bool        incanting   = false;
    std::array<int, GUI_RES_COUNT> inv{};   // last-known inventory (from pin)
    bool        invKnown    = false;
};

struct GuiEgg {
    int         id   = 0;
    int         x    = 0;
    int         y    = 0;
    std::string team;   // resolved from the forking parent at "enw" time
};

// A transient incantation flash, consumed by the renderer for FX.
struct GuiIncantation {
    int  x       = 0;
    int  y       = 0;
    bool resolved = false;   // false=ongoing, true=finished (set by pie)
    bool success  = false;
};

