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

struct GuiState {
    int width    = 0;
    int height   = 0;
    int timeUnit = 100;        // server tick frequency (sgt)
    bool sizeKnown = false;
    float secondsToReset = -1.f;   // countdown to next resource reset (sst); <0 = unknown

    std::vector<std::string>          teams;
    std::vector<GuiTile>              tiles;     // size width*height, index y*width + x
    std::unordered_map<int, GuiPlayer> players;  // keyed by player id
    std::unordered_map<int, GuiEgg>    eggs;     // keyed by egg id

    std::string winner;        // non-empty once a team wins (seg)
    std::deque<GuiLogEntry> log;   // recent player actions (capped); newest at back
    unsigned long logTotal = 0;    // total actions ever logged (for audio event draining)

    // Raw protocol traffic (capped, newest at back). Each entry is prefixed
    // with "<< " (received from the server) or ">> " (sent by this client).
    std::deque<std::string> rawFeed;

    GuiTile &tileAt(int x, int y) { return tiles[y * width + x]; }
    const GuiTile &tileAt(int x, int y) const { return tiles[y * width + x]; }

    bool inBounds(int x, int y) const {
        return x >= 0 && y >= 0 && x < width && y < height;
    }

    void resizeGrid(int w, int h) {
        width = w;
        height = h;
        tiles.assign(static_cast<std::size_t>(w) * h, GuiTile{});
        sizeKnown = true;
    }
};
