#version 450

layout(location = 0) in vec2 inUV;
layout(location = 1) in float inAlpha;
layout(location = 2) flat in uint inType;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 viewProj;
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
    vec4 sunDir;
    vec4 sunColor;
    vec4 skyColorZenith;
    vec4 skyColorHorizon;
    vec4 terrainParams;
    vec4 biomeParams;
    vec4 postParams;
    vec4 atmosphereParams;
    mat4 lightSpaceMatrices[4];
    vec4 cascadeSplits;
    vec4 shadowParams;
    vec4 weatherParams;
    vec4 weatherParams2;
} ubo;

void main() {
    if (inAlpha <= 0.001 || inType == 0u) discard;

    vec2 centered = inUV * 2.0 - 1.0;
    float shapeAlpha = 0.0;
    vec3 col;

    vec3 ambientSky = mix(ubo.skyColorHorizon.rgb, ubo.skyColorZenith.rgb, 0.5) * ubo.skyColorZenith.a + vec3(0.08);
    vec3 lightTint = ambientSky + ubo.sunColor.rgb * ubo.sunColor.a * 0.35 + vec3(0.55, 0.65, 0.85) * ubo.weatherParams2.z;

    if (inType == 1u) {
        // Rain streak
        float radial = (1.0 - centered.x * centered.x) * (1.0 - centered.y * centered.y);
        shapeAlpha = clamp(radial * 0.42, 0.0, 0.42);
        col = lightTint * vec3(0.75, 0.85, 0.98);
    } else {
        // Snow flake
        float d = length(centered);
        shapeAlpha = smoothstep(1.0, 0.25, d) * 0.85;
        col = lightTint * vec3(0.95, 0.97, 1.0);
    }

    outColor = vec4(col, shapeAlpha * inAlpha);
}
