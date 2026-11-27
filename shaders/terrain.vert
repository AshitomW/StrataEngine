#version 450

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

layout(std430, set = 1, binding = 0) readonly buffer TerrainSSBO {
    vec4 data[];
} terrainSSBO;

layout(push_constant) uniform ChunkPC {
    vec4 chunkOffset;
    uvec4 lodParams;
} pc;

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out float outHeight;
layout(location = 3) out float outSlope;
layout(location = 4) out vec2 outUV;
layout(location = 5) flat out uint outLOD;

void main() {
    uint vi = uint(gl_VertexIndex);

    uint lodGridRes = pc.lodParams.x;
    uint lodStep = pc.lodParams.y;
    uint lod = pc.lodParams.z;
    uint totalGridRes = uint(pc.chunkOffset.w);

    uint lodX = vi % lodGridRes;
    uint lodZ = vi / lodGridRes;

    uint ssboX = min(lodX * lodStep, totalGridRes - 1u);
    uint ssboZ = min(lodZ * lodStep, totalGridRes - 1u);
    uint ssboIndex = ssboZ * totalGridRes + ssboX;

    vec4 vertexData = terrainSSBO.data[ssboIndex];

    float cellSize = pc.chunkOffset.z;
    vec2 chunkOrigin = pc.chunkOffset.xy;
    float wx = chunkOrigin.x + float(ssboX) * cellSize;
    float wz = chunkOrigin.y + float(ssboZ) * cellSize;

    // CDLOD continuous vertex morphing toward next coarser grid level
    if (ubo.tracingParams.z > 0.5 && lod < 3u) {
        bool oddX = (lodX & 1u) != 0u;
        bool oddZ = (lodZ & 1u) != 0u;
        if (oddX || oddZ) {
            float maxDist = (lod == 0u) ? 180.0 : ((lod == 1u) ? 360.0 : 620.0);
            float camDist = length(ubo.cameraPos.xz - vec2(wx, wz));
            float morph = smoothstep(maxDist * 0.70, maxDist * 0.98, camDist);
            if (morph > 0.001) {
                uint lx0 = oddX ? (lodX - 1u) : lodX;
                uint lx1 = oddX ? min(lodX + 1u, lodGridRes - 1u) : lodX;
                uint lz0 = oddZ ? (lodZ - 1u) : lodZ;
                uint lz1 = oddZ ? min(lodZ + 1u, lodGridRes - 1u) : lodZ;

                uint sx0 = min(lx0 * lodStep, totalGridRes - 1u);
                uint sx1 = min(lx1 * lodStep, totalGridRes - 1u);
                uint sz0 = min(lz0 * lodStep, totalGridRes - 1u);
                uint sz1 = min(lz1 * lodStep, totalGridRes - 1u);

                vec4 c00 = terrainSSBO.data[sz0 * totalGridRes + sx0];
                vec4 c10 = terrainSSBO.data[sz0 * totalGridRes + sx1];
                vec4 c01 = terrainSSBO.data[sz1 * totalGridRes + sx0];
                vec4 c11 = terrainSSBO.data[sz1 * totalGridRes + sx1];
                vec4 coarseData = 0.25 * (c00 + c10 + c01 + c11);
                vertexData = mix(vertexData, coarseData, morph);
            }
        }
    }

    float height = vertexData.x;
    vec3 normal = normalize(vertexData.yzw);
    vec3 worldPos = vec3(wx, height, wz);

    float slope = 1.0 - clamp(normal.y, 0.0, 1.0);

    outWorldPos = worldPos;
    outNormal = normal;
    outHeight = height;
    outSlope = slope;
    outUV = worldPos.xz * 0.05;
    outLOD = pc.lodParams.z;

    gl_Position = ubo.viewProj * vec4(worldPos, 1.0);
}
