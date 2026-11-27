#version 450

layout(location = 0) out vec2 outUV;
layout(location = 1) out float outAlpha;
layout(location = 2) flat out uint outType;

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

vec3 hash31(uint n) {
    n = (n << 13u) ^ n;
    n = n * (n * n * 15731u + 789221u) + 1376312589u;
    uint h1 = n;
    uint h2 = n * 1664525u + 1013904223u;
    uint h3 = h2 * 1664525u + 1013904223u;
    return vec3(
        float(h1 & 0x00FFFFFFu) / 16777215.0,
        float(h2 & 0x00FFFFFFu) / 16777215.0,
        float(h3 & 0x00FFFFFFu) / 16777215.0
    );
}

void main() {
    float intensity = clamp(ubo.weatherParams.w, 0.0, 1.0);
    uint pType = uint(ubo.weatherParams2.w + 0.5);
    uint inst = uint(gl_InstanceIndex);

    vec3 rnd = hash31(inst + 17u);
    if (rnd.x > intensity || pType == 0u) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        outUV = vec2(0.0);
        outAlpha = 0.0;
        outType = 0u;
        return;
    }

    float time = ubo.terrainParams.z;
    float windSpeed = ubo.weatherParams2.x;
    vec3 boxSize = vec3(56.0, 36.0, 56.0);

    vec3 fallVel = (pType == 1u)
        ? vec3(windSpeed * 0.45, -24.0, windSpeed * 0.28)
        : vec3(windSpeed * 0.25 + sin(time * 1.8 + rnd.y * 12.0) * 1.2, -3.5, windSpeed * 0.15 + cos(time * 1.5 + rnd.z * 12.0) * 1.2);

    vec3 basePos = rnd * boxSize + fallVel * time;
    vec3 relPos = mod(basePos - ubo.cameraPos.xyz, boxSize) - boxSize * 0.5;
    vec3 worldCenter = ubo.cameraPos.xyz + relPos;

    // Hide particles below water level
    if (worldCenter.y < ubo.terrainParams.x) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        outUV = vec2(0.0);
        outAlpha = 0.0;
        outType = 0u;
        return;
    }

    const vec2 quadCorners[6] = vec2[6](
        vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0),
        vec2(-1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, 1.0)
    );
    vec2 corner = quadCorners[uint(gl_VertexIndex) % 6u];

    vec3 toCam = normalize(ubo.cameraPos.xyz - worldCenter);
    vec3 upAxis = (pType == 1u) ? normalize(-fallVel) : vec3(0.0, 1.0, 0.0);
    vec3 rightAxis = normalize(cross(upAxis, toCam));
    if (pType == 2u) {
        upAxis = normalize(cross(toCam, rightAxis));
    }

    vec2 size = (pType == 1u) ? vec2(0.018, 0.36) : vec2(0.065, 0.065);
    vec3 worldPos = worldCenter + rightAxis * (corner.x * size.x) + upAxis * (corner.y * size.y);

    float dist = length(relPos);
    outAlpha = (1.0 - smoothstep(14.0, 28.0, dist)) * smoothstep(0.5, 2.0, dist);
    outUV = corner * 0.5 + 0.5;
    outType = pType;

    gl_Position = ubo.viewProj * vec4(worldPos, 1.0);
}
