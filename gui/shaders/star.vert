#version 330 core
// Camera-facing billboard for a glowing star. Expands a unit quad (corners in
// [-1,1]) around a world-space centre using the camera's right/up axes.
layout(location = 0) in vec2 aCorner;
