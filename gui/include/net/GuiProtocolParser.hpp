#pragma once

#include "net/GuiState.hpp"
#include <string>

// Applies one line of the GUI protocol to a GuiState. No socket, no rendering:
// it can be tested with plain strings.
class GuiProtocolParser {
public:
    // Returns false for a line it does not understand (ignored, the state is unchanged).
    static bool apply(const std::string &line, GuiState &state);
};
