#include "render/Mesh.hpp"
#include <GL/glew.h>
#include <cmath>
#include <stdexcept>

Mesh::Mesh(const std::vector<float>& verts, const std::vector<unsigned int>& indices)
    : indexCount((int)indices.size())
{
    // Stride: 9 floats (pos + normal + vertColor) or 6 floats (pos + normal)
    const int stride = ((int)verts.size() % 9 == 0 && (int)verts.size() % 6 != 0)
                       ? 9 : 6;
    _stride = stride;

    if (!verts.empty()) {
        float sumX = 0, sumY = 0, sumZ = 0;
        int   n    = (int)verts.size() / stride;
        for (int i = 0; i < n; ++i) {
            sumX += verts[i*stride + 0];
            sumY += verts[i*stride + 1];
            sumZ += verts[i*stride + 2];
        }
        _centroid = {sumX/n, sumY/n, sumZ/n};

        float maxR2 = 0;
        for (int i = 0; i < n; ++i) {
            float dx = verts[i*stride+0] - _centroid.x;
            float dy = verts[i*stride+1] - _centroid.y;
            float dz = verts[i*stride+2] - _centroid.z;
            float r2 = dx*dx + dy*dy + dz*dz;
            if (r2 > maxR2) maxR2 = r2;
        }
