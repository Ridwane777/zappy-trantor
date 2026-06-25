#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform mat4  uModel;
uniform mat4  uView;
uniform mat4  uProj;
uniform mat3  uNormalMat;
uniform float uOutlineWidth;
uniform float uTime;
uniform bool  uSway;

void main() {
    vec3 pos = aPos;
    if (uSway) {
        float weight = max(0.0, pos.y) * 0.6;
        pos.x += sin(uTime * 1.3 + pos.z * 0.5) * weight * 0.12;
        pos.z += cos(uTime * 0.9 + pos.x * 0.5) * weight * 0.08;
