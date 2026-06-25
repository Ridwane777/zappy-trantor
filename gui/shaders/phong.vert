#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aVertColor;

uniform mat4  uModel;
uniform mat4  uView;
uniform mat4  uProj;
uniform mat3  uNormalMat;
uniform float uTime;
uniform bool  uWave;
uniform bool  uSway;

out vec3  vPos;
out vec3  vNormal;
out float vWaveH;
out vec3  vVertColor;
out vec2  vTexCoord;

void main() {
    vec3 pos = aPos;
    vec3 norm = aNormal;

    if (uWave) {
        // Overlapping sine waves on XZ for ocean surface
        float wx = pos.x * 0.9 + uTime * 1.6;
        float wz = pos.z * 0.7 + uTime * 1.1 + 1.3;
        float wd = (pos.x + pos.z) * 0.5 + uTime * 2.0;
        float h  = sin(wx) * 0.10
                 + cos(wz) * 0.07
                 + sin(wd) * 0.04;
        pos.y += h;
        vWaveH = h;
        // Approximate wave normal from gradient
        float dhdx = cos(wx) * 0.9 * 0.10 + cos(wd) * 0.5 * 0.04;
        float dhdz = -sin(wz) * 0.7 * 0.07 + cos(wd) * 0.5 * 0.04;
        norm = normalize(vec3(-dhdx, 1.0, -dhdz));
    }

    if (uSway) {
        // Sway top vertices more than base (use local Y as weight)
        float weight = max(0.0, pos.y) * 0.6;
