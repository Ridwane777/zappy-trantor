#version 330 core

in vec3  vPos;
in vec3  vNormal;
in float vWaveH;
in vec3  vVertColor;
in vec2  vTexCoord;

uniform vec3  uCamPos;
uniform vec3  uColor;
uniform vec3  uColor2;     // mid-zone color
uniform vec3  uColor3;     // bottom-zone color
uniform float uSplit1;     // world Y: above = uColor, below = uColor2
uniform float uSplit2;     // world Y: below = uColor3
uniform bool  uMultiColor;    // enable zone coloring
uniform bool      uUseVertexColor;
uniform sampler2D uTexture;
uniform bool      uUseTexture;
uniform float uShininess;
uniform float uAmbientStrength;
uniform bool  uEmissive;
uniform bool  uCrystal;
uniform bool  uWave;
uniform float uAlpha;
uniform bool  uCircleClip;
uniform bool  uShoreBlend;
uniform vec2  uClipCenter;
uniform float uClipRadius;
uniform float uShoreInnerR;
uniform float uShoreWidth;

uniform vec3 uDirLightDir;
uniform vec3 uDirLightColor;

uniform vec3 uPointLight0Pos;
uniform vec3 uPointLight0Color;
uniform vec3 uPointLight1Pos;
uniform vec3 uPointLight1Color;

out vec4 FragColor;

void main() {
    if (uEmissive) {
        FragColor = vec4(uColor * 1.4, 1.0);
        return;
    }

    // pick base color: texture > vertex-baked > zone > uniform
    vec3 baseColor = uColor;
    if      (uUseTexture)     baseColor = texture(uTexture, vTexCoord).rgb;
    else if (uUseVertexColor) baseColor = vVertColor;
    if (!uUseTexture && !uUseVertexColor && uMultiColor) {
        if      (vPos.y < uSplit2) baseColor = uColor3;
        else if (vPos.y < uSplit1) baseColor = uColor2;
    }

    vec3 N = normalize(vNormal);
    vec3 L = normalize(-uDirLightDir);
    vec3 V = normalize(uCamPos - vPos);

    // Lambert diffuse
    float diff   = max(dot(N, L), 0.0);
    vec3  diffuse = baseColor * uDirLightColor * diff;
    vec3  ambient = baseColor * uAmbientStrength;
    vec3  col     = ambient + diffuse;

    if (uCrystal) {
        // Blinn-Phong specular
        vec3  H    = normalize(L + V);
        float spec = pow(max(dot(N, H), 0.0), uShininess);
        vec3 specColor = mix(vec3(1.0), baseColor, 0.25);
        col += specColor * spec * 2.0;

        // Fresnel rim
        float fresnel = pow(1.0 - max(dot(N, V), 0.0), 2.5);
        col += baseColor * fresnel * 0.9;
