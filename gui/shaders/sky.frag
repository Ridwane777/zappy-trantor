#version 330 core
in float vY;
in vec2  vNDC;

uniform vec3  uSkyTop;
uniform vec3  uSkyHorizon;
uniform mat4  uInvViewProj;
uniform vec3  uCamPos;
uniform vec3  uSunDir;        // normalized, points FROM scene TO sun (world space)
uniform vec3  uMoonDir;       // points FROM scene TO moon
uniform float uSunStrength;   // 1 day, 0 night
uniform float uMoonStrength;  // 0 day, 1 night

out vec4 FragColor;

float hash(vec3 p) {
    return fract(sin(dot(floor(p), vec3(12.9898, 78.233, 37.719))) * 43758.5453);
}

void main() {
    float t = pow(max(vY, 0.0), 0.65);
    vec3 sky = mix(uSkyHorizon, uSkyTop, t);

    vec4 clip  = vec4(vNDC, 1.0, 1.0);
    vec4 world = uInvViewProj * clip;
    world /= world.w;
    vec3 rayDir = normalize(world.xyz - uCamPos);

    // ── sun (fades out at night) ──
    float cosA   = dot(rayDir, uSunDir);
    float disc   = smoothstep(0.9985, 0.9998, cosA);
    float corona = smoothstep(0.975,  0.9985, cosA) * 0.55;
    float glow   = smoothstep(0.90,   0.975,  cosA) * 0.35;
    vec3  sunColor  = vec3(1.0, 0.75, 0.20);
