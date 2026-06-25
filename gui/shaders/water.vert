#version 330 core
// Horizontal water plane in the XZ plane at y=0. Corner in [-1,1].
layout(location = 0) in vec2 aCorner;

uniform mat4  uView;
uniform mat4  uProj;
uniform float uSize;

out vec2 vUV;
out vec2 vXZ;

void main() {
