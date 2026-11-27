#version 450

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;

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

layout(set = 2, binding = 0) uniform sampler2D uShadowAtlas;

const float PI = 3.14159265359;

float sampleShadow(vec3 worldPos, vec3 N, vec3 L, float viewDepth) {
    float strength = ubo.shadowParams.x;
    if (strength <= 0.001) return 1.0;

    int cascade = 3;
    if (viewDepth < ubo.cascadeSplits.x) cascade = 0;
    else if (viewDepth < ubo.cascadeSplits.y) cascade = 1;
    else if (viewDepth < ubo.cascadeSplits.z) cascade = 2;

    float normalOffset = 0.06 * float(cascade + 1);
    vec4 lsPos = ubo.lightSpaceMatrices[cascade] * vec4(worldPos + N * normalOffset, 1.0);
    vec3 proj = lsPos.xyz / lsPos.w;
    vec2 tileUV = proj.xy * 0.5 + 0.5;
    if (any(lessThan(tileUV, vec2(0.01))) || any(greaterThan(tileUV, vec2(0.99))) || proj.z > 1.0) {
        return 1.0;
    }

    vec2 tileOffset = vec2(float(cascade % 2), float(cascade / 2)) * 0.5;
    vec2 atlasUV = tileOffset + tileUV * 0.5;
    float bias = ubo.shadowParams.y * float(cascade + 1);

    vec2 texel = vec2(1.0 / 4096.0);
    float shadow = 0.0;
    vec2 offsets[4] = vec2[4](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, 1.0));
    for (int i = 0; i < 4; ++i) {
        float d = texture(uShadowAtlas, atlasUV + offsets[i] * texel).r;
        shadow += ((proj.z - bias) <= d) ? 1.0 : 0.0;
    }
    shadow *= 0.25;
    float distFade = 1.0 - smoothstep(ubo.cascadeSplits.w * 0.80, ubo.cascadeSplits.w, viewDepth);
    return mix(1.0, shadow, strength * distFade);
}

void main() {
    vec3 N = normalize(inNormal);
    vec3 L = normalize(ubo.sunDir.xyz);
    vec3 D = normalize(inWorldPos - ubo.cameraPos.xyz);
    vec3 V = -D;

    float dist = length(ubo.cameraPos.xyz - inWorldPos);
    float maxDist = max(ubo.biomeParams.w, 150.0);
    float distFactor = clamp(dist / maxDist, 0.0, 1.0);

    if (distFactor >= 0.94) {
        discard;
    }

    float sunIntensity = ubo.atmosphereParams.z;
    float iblStrength = ubo.atmosphereParams.w;
    vec3 sunRadiance = ubo.sunColor.rgb * sunIntensity;

    float NdotL = max(dot(N, L), 0.0);
    float wrapLight = pow(clamp(dot(N, L) * 0.4 + 0.6, 0.0, 1.0), 1.8) * 0.25;

    // Back-lit subsurface scattering translucency through foliage
    vec3 backDir = normalize(L - N * 0.35);
    float sss = pow(clamp(dot(V, -backDir), 0.0, 1.0), 3.0) * 0.45 * ubo.sunColor.a;

    // Subtle specular sheen
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), 24.0) * 0.06 * step(0.0, NdotL);

    float viewDepth = max(-(ubo.view * vec4(inWorldPos, 1.0)).z, 0.0);
    float shadow = sampleShadow(inWorldPos, N, L, viewDepth);

    vec3 directLight = (sunRadiance * ((NdotL * 0.85 + wrapLight) / PI + sss / PI) + sunRadiance * spec) * shadow;

    vec3 skyAmb = ubo.skyColorZenith.rgb * ubo.skyColorZenith.a * 1.65;
    vec3 groundAmb = ubo.skyColorHorizon.rgb * 0.42 * ubo.skyColorZenith.a;
    vec3 ambient = mix(groundAmb, skyAmb, N.y * 0.5 + 0.5) * iblStrength;

    vec3 litColor = inColor * (ambient + directLight);

    float waterHeight = ubo.terrainParams.x;
    float heightHaze = exp(-max(inWorldPos.y - waterHeight, 0.0) * 0.035) * 0.22;
    float valleyFog = heightHaze * clamp(dist / (maxDist * 0.4), 0.0, 1.0);

    float expFog = 1.0 - exp(-pow(distFactor * 1.7, 2.0));
    float edgeFade = smoothstep(0.70, 0.94, distFactor);
    float distFog = clamp(max(expFog, edgeFade), 0.0, 1.0);
    float fogFactor = clamp(distFog + valleyFog, 0.0, 1.0);

    float elevation = clamp(D.y * 0.75 + 0.25, 0.0, 1.0);
    vec3 horizonAtmosphere = mix(ubo.skyColorHorizon.rgb, ubo.skyColorZenith.rgb, elevation);

    float sunDot = max(dot(D, L), 0.0);
    float g = clamp(ubo.atmosphereParams.y, 0.1, 0.95);
    float mie = (1.0 - g * g) / pow(max(1.0 + g * g - 2.0 * g * sunDot, 1e-4), 1.5) * 0.06;
    float sunGlow = pow(sunDot, 16.0) * 0.20 + mie * 0.65;
    vec3 fogColor = horizonAtmosphere + ubo.sunColor.rgb * sunGlow * sunIntensity * 0.45 * ubo.sunColor.a;

    vec3 finalColor = mix(litColor, fogColor, fogFactor);
    outColor = vec4(max(finalColor, vec3(0.0)), 1.0);
}
