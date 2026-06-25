#version 330 core
in vec3 vNormal;
in vec3 vWorldPos;
in vec2 vUV;

uniform vec3  uColor;
uniform float uAlpha;
uniform vec3  uCamPos;
uniform bool  uPillarMode;
// ring uniforms
uniform vec3  uRingCenter;
uniform float uRingInnerR;
