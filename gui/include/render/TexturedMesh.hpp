#pragma once
// A static mesh that carries UVs and a diffuse texture, for OBJ models exported
// with texture coordinates (e.g. the menu logo). Separate from Mesh, which only
// stores pos+normal and is used by the game world.

#include <string>
#include <glm/glm.hpp>

class TexturedMesh {
public:
    TexturedMesh() = default;
    ~TexturedMesh();

    TexturedMesh(const TexturedMesh &) = delete;
    TexturedMesh &operator=(const TexturedMesh &) = delete;
    TexturedMesh(TexturedMesh &&other) noexcept;
    TexturedMesh &operator=(TexturedMesh &&other) noexcept;

