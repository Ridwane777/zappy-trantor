#version 330 core
in vec3 vPos;
in vec3 vNormal;
in vec2 vUV;

uniform sampler2D uTex;
uniform vec3  uDirLightDir;
uniform vec3  uDirLightColor;
uniform vec3  uPointPos;     // explosion flash position
uniform vec3  uPointColor;   // flash colour * intensity
uniform float uAmbient;
uniform float uAlpha;        // for the reflection fade
uniform float uBright;       // overall brightness multiplier (reflection dims it)

out vec4 FragColor;

void main() {
