#version 330 core
// Soft radial pool of light, drawn with additive blending (GL_ONE, GL_ONE).
in vec2 vUV;

uniform vec3  uColor;
uniform float uIntensity;

