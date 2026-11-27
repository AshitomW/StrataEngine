#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inMatParams; // x: roughness, y: emissive

layout(location = 4) in vec4 inInstancePosScale;
layout(location = 5) in vec4 inInstanceParams; // x: yaw, y: type, z: tintVar, w: unused

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outColor;
layout(location = 3) out vec2 outMatParams;

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
    vec4 tracingParams;
} ubo;

void main() {
    float scale = inInstancePosScale.w;
    float rot = inInstanceParams.x;
    float tint = inInstanceParams.z;

    mat2 rotMat = mat2(cos(rot), -sin(rot), sin(rot), cos(rot));

    vec3 localPos = inPosition * scale;
    localPos.xz = rotMat * localPos.xz;

    vec3 localNorm = inNormal;
    localNorm.xz = rotMat * localNorm.xz;

    vec3 worldPos = localPos + inInstancePosScale.xyz;

    outWorldPos = worldPos;
    outNormal = normalize(localNorm);
    outColor = inColor * (1.0 + tint * 0.14);
    outMatParams = inMatParams;

    gl_Position = ubo.viewProj * vec4(worldPos, 1.0);
}
