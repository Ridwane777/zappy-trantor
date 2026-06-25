#version 330 core
// Glowing star: hot white core + soft coloured halo + 4-point spikes.
// Drawn with additive blending (GL_ONE, GL_ONE).
in vec2 vUV;

uniform vec3  uColor;
uniform float uIntensity;

out vec4 FragColor;

void main() {
    float r = length(vUV);

    float core = exp(-r * r * 11.0);          // tight, bright centre
    float halo = exp(-r * 2.6) * 0.40;        // soft surrounding glow
