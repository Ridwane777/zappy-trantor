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

