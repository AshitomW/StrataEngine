#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;

layout(location = 3) in vec4 inInstancePos;
layout(location = 4) in vec4 inInstanceParams;

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

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outColor;

void main() {
    float scale = inInstancePos.w;
    float rot = inInstanceParams.x;
    uint objType = uint(inInstanceParams.y);
    float colorVar = inInstanceParams.z;
    float time = ubo.terrainParams.z;

    mat2 rotMat = mat2(
        cos(rot), -sin(rot),
        sin(rot),  cos(rot)
    );

    vec3 localPos = inPosition * scale;
    localPos.xz = rotMat * localPos.xz;

    vec3 localNormal = inNormal;
    localNormal.xz = rotMat * localNormal.xz;

    if (objType == 0u || objType == 1u || objType == 3u) {
        float windScale = clamp(ubo.weatherParams2.x * 0.14, 0.35, 3.0);
        float windStrength = ((objType == 3u) ? 0.35 : 0.15) * windScale;
        float windSway = sin(time * (2.5 + windScale * 0.8) + inInstancePos.x * 0.35 + inInstancePos.z * 0.25) * windStrength * max(0.0, inPosition.y);
        localPos.x += windSway;
        localPos.z += windSway * 0.5;
    }

    vec3 worldPos = localPos + inInstancePos.xyz;

    outWorldPos = worldPos;
    outNormal = normalize(localNormal);
    outColor = inColor * (1.0 + colorVar * 0.20);

    gl_Position = ubo.viewProj * vec4(worldPos, 1.0);
}
