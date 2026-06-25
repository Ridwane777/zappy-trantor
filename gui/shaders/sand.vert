#version 330 core
// Flat ground disc in the XZ plane, corner in [-1,1], soft-faded edges.
layout(location = 0) in vec2 aCorner;

uniform mat4  uView;
uniform mat4  uProj;
uniform float uSize;
uniform float uY;

out vec2 vXZ;

void main() {
    vXZ = aCorner;
    gl_Position = uProj * uView * vec4(aCorner.x * uSize, uY, aCorner.y * uSize, 1.0);
}
