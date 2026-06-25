#version 330 core
layout(location = 0) in vec2 aPos;
out float vY;
out vec2  vNDC;
void main() {
    vY   = aPos.y * 0.5 + 0.5;
