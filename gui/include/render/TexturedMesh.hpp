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

    // Load geometry (v/vt/vn) from `objPath` and the diffuse map from `texPath`.
    // Throws std::runtime_error on failure.
    static TexturedMesh load(const std::string &objPath, const std::string &texPath);

    void draw() const;   // binds texture (unit 0) + VAO, issues the draw

    glm::vec3 centroid()       const { return _centroid; }
    float     boundingRadius() const { return _radius; }
    bool      hasTexture()     const { return _texId != 0; }

private:
    unsigned int _vao = 0, _vbo = 0, _ebo = 0, _texId = 0;
    int          _indexCount = 0;
    glm::vec3    _centroid{0.f};
    float        _radius = 1.f;

    void cleanup();
};
