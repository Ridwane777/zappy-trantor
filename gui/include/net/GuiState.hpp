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
