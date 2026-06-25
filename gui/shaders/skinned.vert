#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aJoints;
layout(location = 3) in vec4 aWeights;
layout(location = 4) in vec2 aTexCoord;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform mat3 uNormalMat;

const int MAX_BONES = 96;
uniform mat4 uBones[96];

out vec3  vPos;
out vec3  vNormal;
out float vWaveH;
out vec3  vVertColor;
out vec2  vTexCoord;
