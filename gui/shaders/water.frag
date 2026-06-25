#version 330 core
in vec2 vUV;
in vec2 vXZ;

uniform sampler2D uTex;
uniform float uTime;
uniform float uAlpha;
uniform vec2  uSunXZ;    // horizontal direction toward the sun
uniform vec3  uSunCol;   // warm sun colour

out vec4 FragColor;

void main() {
    // two scrolling layers of the water texture for a rippling surface
