#version 330 core
// Camera-facing billboard for a glowing star. Expands a unit quad (corners in
// [-1,1]) around a world-space centre using the camera's right/up axes.
layout(location = 0) in vec2 aCorner;

uniform vec3  uCenter;
uniform float uSize;
uniform mat4  uView;
uniform mat4  uProj;

out vec2 vUV;

void main() {
    vec3 right = vec3(uView[0][0], uView[1][0], uView[2][0]);
    vec3 up    = vec3(uView[0][1], uView[1][1], uView[2][1]);
    vec3 world = uCenter + (aCorner.x * right + aCorner.y * up) * uSize;
    vUV = aCorner;
    gl_Position = uProj * uView * vec4(world, 1.0);
}
