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
