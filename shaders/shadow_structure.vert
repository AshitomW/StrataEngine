#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inMatParams;

layout(location = 4) in vec4 inInstancePosScale;
layout(location = 5) in vec4 inInstanceParams;

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

layout(push_constant) uniform ShadowPC {
    uvec4 cascadeInfo;
} pc;

void main() {
    float scale = inInstancePosScale.w;
    float rot = inInstanceParams.x;
    mat2 rotMat = mat2(cos(rot), -sin(rot), sin(rot), cos(rot));

    vec3 localPos = inPosition * scale;
    localPos.xz = rotMat * localPos.xz;
    vec3 worldPos = localPos + inInstancePosScale.xyz;

    uint cascade = min(pc.cascadeInfo.x, 3u);
    gl_Position = ubo.lightSpaceMatrices[cascade] * vec4(worldPos, 1.0);
}
