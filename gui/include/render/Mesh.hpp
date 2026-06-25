#pragma once
#include <vector>
#include <glm/glm.hpp>

class Mesh {
public:
    // verts: interleaved vec3 pos + vec3 normal (6 floats per vertex)
    Mesh(const std::vector<float>& verts, const std::vector<unsigned int>& indices);
    ~Mesh();
