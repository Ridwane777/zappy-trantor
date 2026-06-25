#version 330 core
in vec3 vPos;
uniform vec3  uGlowCenter;
uniform float uGlowRadius;
uniform vec3  uColor;
uniform float uAlpha;
out vec4 FragColor;
void main() {
    float d = length(vPos - uGlowCenter) / uGlowRadius;
