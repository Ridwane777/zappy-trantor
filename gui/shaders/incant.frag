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

    float intensity;

    if (uPillarMode) {
        // ── Phong-analogy beam: diffuse (wide Gaussian) + specular (narrow core)
        float cx   = (vUV.x - 0.5) * 2.0;            // -1 → +1 across width
        float diff = exp(-cx * cx * 6.0);              // wide soft envelope
        float spec = exp(-cx * cx * 55.0) * 0.60;      // sharp bright core
        float hFade = pow(max(1.0 - vUV.y, 0.0), 1.4); // fade from ground to top
        // Fresnel makes beam brighter when viewed from the side
        intensity = (diff + spec) * hFade * (0.50 + 0.50 * fresnel);
    } else {
