#version 330 core
in vec3 vPos;
in vec3 vNormal;
in vec2 vUV;

uniform sampler2D uTex;
uniform vec3  uDirLightDir;
uniform vec3  uDirLightColor;
