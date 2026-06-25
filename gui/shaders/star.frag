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

    // 4-point star spikes (bright along the two axes, falling off across them)
    float sx = pow(max(0.0, 1.0 - abs(vUV.x)), 2.0) * exp(-abs(vUV.y) * 16.0);
    float sy = pow(max(0.0, 1.0 - abs(vUV.y)), 2.0) * exp(-abs(vUV.x) * 16.0);
    float spikes = (sx + sy) * 0.55;

    float i = (core + halo + spikes) * uIntensity;
