#version 330 core
// Glowing star: hot white core + soft coloured halo + 4-point spikes.
// Drawn with additive blending (GL_ONE, GL_ONE).
in vec2 vUV;

uniform vec3  uColor;
uniform float uIntensity;
