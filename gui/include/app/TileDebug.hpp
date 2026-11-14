#pragma once
#include <glm/glm.hpp>
#include <SFML/Graphics.hpp>

struct MapData;
struct GuiState;

void drawTileGridGL(const glm::mat4& view, const glm::mat4& proj,
                    const MapData& map, const GuiState& gs, float cx, float cz);

// Server tile under the screen point (mx, my), by casting a ray onto the
// ground plane. Returns false when the ray misses the ground.
bool screenToTile(const glm::mat4& view, const glm::mat4& proj,
                  unsigned winW, unsigned winH, int mx, int my,
                  const MapData& map, const GuiState& gs, int& tx, int& ty);

void drawTileGridHUD(const glm::mat4& view, const glm::mat4& proj,
                     const MapData& map, const GuiState& gs,
                     float cx, float cz,
                     sf::RenderWindow& win, const sf::Font& font,
                     unsigned winW, unsigned winH,
                     int mouseX, int mouseY);
