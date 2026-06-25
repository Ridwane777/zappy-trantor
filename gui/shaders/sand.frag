#version 330 core
in vec2 vXZ;

uniform vec3  uColor;     // sand colour (near)
uniform vec3  uColorFar;  // tint blended toward the horizon
uniform float uAlpha;

out vec4 FragColor;

