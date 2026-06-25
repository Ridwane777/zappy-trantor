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
