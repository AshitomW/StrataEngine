#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec2 outUV;
layout(location = 2) out float outWaveHeight;
layout(location = 3) out float outFoamFactor;
layout(location = 4) out vec3 outUndisplacedPos;

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 viewProj;
    mat4 view;
    mat4 proj;
    mat4 invViewProj;
    mat4 invProj;
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

layout(set = 2, binding = 0) uniform sampler2D uWaveDispFoam;
layout(set = 2, binding = 1) uniform sampler2D uRipples;

layout(push_constant) uniform WaterPC {
    vec4 waterParams1; // x: waterHeight, y: waveAmplitude, z: waveSpeed, w: time
    vec4 waterParams2; // x: clarity, y: windSpeed, z: choppiness, w: debugMode
    vec4 gridCenter;   // x: centerX, y: centerZ, z: rippleCenterX, w: rippleCenterZ
} pc;

void main() {
    float waterHeight = pc.waterParams1.x;
    vec2 centerXZ = pc.gridCenter.xy;

    vec3 basePos = vec3(inPosition.x + centerXZ.x, waterHeight, inPosition.z + centerXZ.y);
    outUndisplacedPos = basePos;

    // Two-octave non-repeating spectral wave lookup
    vec2 uv1 = basePos.xz / 64.0;
    vec2 uv2 = basePos.xz / 23.0 + vec2(0.37, 0.71);
    vec4 sim1 = textureLod(uWaveDispFoam, uv1, 0.0);
    vec4 sim2 = textureLod(uWaveDispFoam, uv2, 0.0);

    vec3 disp = sim1.xyz * 0.72 + sim2.xyz * 0.28;
    float foam = clamp(sim1.w * 0.65 + sim2.w * 0.55, 0.0, 1.0);

    // Distance attenuation beyond 900m to prevent horizon vertex aliasing
    float camDist = length(basePos.xz - ubo.cameraPos.xz);
    float distFade = 1.0 - smoothstep(450.0, 1400.0, camDist);
    disp *= distFade;

    // Interactive shallow-water ripples near camera
    vec2 ripUV = (basePos.xz - pc.gridCenter.zw) / 48.0 + 0.5;
    if (all(greaterThan(ripUV, vec2(0.02))) && all(lessThan(ripUV, vec2(0.98)))) {
        float edgeMask = smoothstep(0.02, 0.15, ripUV.x) * smoothstep(0.98, 0.85, ripUV.x) *
                         smoothstep(0.02, 0.15, ripUV.y) * smoothstep(0.98, 0.85, ripUV.y);
        float ripH = textureLod(uRipples, ripUV, 0.0).r;
        disp.y += ripH * 0.35 * edgeMask;
    }

    vec3 worldPos = basePos + disp;

    outWorldPos = worldPos;
    outUV = worldPos.xz * 0.05;
    outWaveHeight = disp.y;
    outFoamFactor = foam;

    gl_Position = ubo.viewProj * vec4(worldPos, 1.0);
}
