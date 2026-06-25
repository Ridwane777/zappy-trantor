#pragma once
// 3D "alive" startup screen.
//
// Sequence: a small emissive light falls from the top to screen center, bursts
// into a 3D particle explosion that flashes a point light, then the ZAPPY logo
// is revealed with a mirrored reflection beneath it. Two 2D buttons (Start,
// Parameters) sit below. Start returns the chosen connection config; Parameters
// opens an editable host/port panel.
//
// Runs its own GL resources + event loop, so it fully owns the window until it
// returns. The caller then proceeds to connect + run the game renderer.

#include <SFML/Graphics.hpp>
#include <string>

struct MenuConfig {
    std::string host = "127.0.0.1";
    int         port = 4242;
};

// Runs the startup screen until the user starts or quits.
//   returns true  -> user pressed Start; `out` holds the chosen host/port.
//   returns false -> user closed the window / quit.
// Requires a current OpenGL context (GLEW already initialised) on `window`.
// `font` is the display font for the menu; `uiFont` is the in-game font used for
// the settings panel (readable numerals for host/port).
bool runMenu(sf::RenderWindow &window, sf::Font &font, sf::Font &uiFont,
             MenuConfig &out);

// Renders one loading-screen frame: a title, a progress bar at `progress`
// (0..1) and a status line. Call repeatedly between asset-load steps. Pumps
// window events so the window stays responsive during the load.
