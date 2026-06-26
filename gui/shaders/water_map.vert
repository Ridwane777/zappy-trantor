#version 330 core
// Shore-water ripple shader for the map ground meshes (same VAO layout as
// phong.vert: pos/normal/vertcolor), kept separate so the texture/sun-glitter
// sampling in the fragment stage doesn't have to branch inside phong.frag.
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aVertColor;

uniform mat4  uModel;
uniform mat4  uView;
uniform mat4  uProj;
uniform mat3  uNormalMat;
uniform float uTime;
uniform bool  uWave;

out vec3 vWorldPos;
out vec3 vNormal;

void main() {
    vec3 pos  = aPos;
    vec3 norm = aNormal;

    if (uWave) {
        // Same overlapping-sine ripple as phong.vert's ocean wave, so the
        // shore water's geometry motion matches the deep ocean around it.
        float wx = pos.x * 0.9 + uTime * 1.6;
        float wz = pos.z * 0.7 + uTime * 1.1 + 1.3;
        float wd = (pos.x + pos.z) * 0.5 + uTime * 2.0;
        float h  = sin(wx) * 0.10 + cos(wz) * 0.07 + sin(wd) * 0.04;
        pos.y += h;
        float dhdx = cos(wx) * 0.9 * 0.10 + cos(wd) * 0.5 * 0.04;
        float dhdz = -sin(wz) * 0.7 * 0.07 + cos(wd) * 0.5 * 0.04;
        norm = normalize(vec3(-dhdx, 1.0, -dhdz));
    }

    vec4 wp     = uModel * vec4(pos, 1.0);
    vWorldPos   = wp.xyz;
    vNormal     = normalize(uNormalMat * norm);
    gl_Position = uProj * uView * wp;
}
