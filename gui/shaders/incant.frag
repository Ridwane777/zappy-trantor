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
uniform float uRingOuterR;

out vec4 FragColor;

void main() {
    vec3  viewDir = normalize(uCamPos - vWorldPos);
    float cosV    = abs(dot(normalize(vNormal), viewDir));

    // Fresnel: 0 when facing camera directly, 1 at grazing angle
    // Matches raytracer reflectDir: I - 2(I·N)N → same perp-view brightening
    float fresnel = 1.0 - cosV;
    fresnel       = fresnel * fresnel * fresnel;   // cubic: sharp grazing peak
