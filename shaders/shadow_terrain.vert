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
    uvec4 lodParams; // w = cascadeIndex
} pc;

void main() {
    uint vi = uint(gl_VertexIndex);

    uint lodGridRes = pc.lodParams.x;
    uint lodStep = pc.lodParams.y;
    uint totalGridRes = uint(pc.chunkOffset.w);

    uint lodX = vi % lodGridRes;
    uint lodZ = vi / lodGridRes;

    uint ssboX = min(lodX * lodStep, totalGridRes - 1u);
    uint ssboZ = min(lodZ * lodStep, totalGridRes - 1u);
    uint ssboIndex = ssboZ * totalGridRes + ssboX;

    float height = terrainSSBO.data[ssboIndex].x;
    float cellSize = pc.chunkOffset.z;
    vec2 chunkOrigin = pc.chunkOffset.xy;

    vec3 worldPos = vec3(
        chunkOrigin.x + float(ssboX) * cellSize,
        height,
        chunkOrigin.y + float(ssboZ) * cellSize
    );

    uint cascade = min(pc.lodParams.w, 3u);
    gl_Position = ubo.lightSpaceMatrices[cascade] * vec4(worldPos, 1.0);
}
