#pragma once
#include <vector>
#include <glm/glm.hpp>

class Mesh {
public:
    // verts: interleaved vec3 pos + vec3 normal (6 floats per vertex)
    Mesh(const std::vector<float>& verts, const std::vector<unsigned int>& indices);
    ~Mesh();

    // Non-copyable
    Mesh(const Mesh&)            = delete;
    Mesh& operator=(const Mesh&) = delete;

    // Movable
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    void draw() const;
    int  vertCount() const;

    glm::vec3 centroid()       const;
    float     boundingRadius() const;
    float     yMin()           const;
    bool      hasVertexColor() const { return _stride == 9; }

private:
    unsigned int vao = 0;
    unsigned int vbo = 0;
