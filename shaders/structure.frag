#version 450

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inMatParams;

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

float D_GGX(float NdotH, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / max(PI * denom * denom, 1e-6);
}

float G_SchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) * 0.125;
    return NdotV / max(NdotV * (1.0 - k) + k, 1e-6);
}

float G_Smith(float NdotV, float NdotL, float roughness) {
    return G_SchlickGGX(NdotV, roughness) * G_SchlickGGX(NdotL, roughness);
}

vec3 F_Schlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float sampleStructureShadow(vec3 worldPos, vec3 N, vec3 L, float viewDepth) {
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
    vec2 offs[4] = vec2[4](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, 1.0));
    for (int i = 0; i < 4; ++i) {
        float d = texture(uShadowAtlas, atlasUV + offs[i] * texel).r;
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
    vec3 H = normalize(L + V);

    float dist = length(ubo.cameraPos.xyz - inWorldPos);
    float maxDist = max(ubo.biomeParams.w, 150.0);
    float distFactor = clamp(dist / maxDist, 0.0, 1.0);
    if (distFactor >= 0.94) discard;

    vec3 albedo = inColor;
    float roughness = clamp(inMatParams.x, 0.08, 0.98);
    float emissive = inMatParams.y;

    // Weather snow & wetness coating on structures
    float wetness = clamp(ubo.weatherParams.y, 0.0, 1.0);
    float snow = clamp(ubo.weatherParams.z, 0.0, 1.0);
    if (snow > 0.01 && emissive < 0.1) {
        float snowMask = smoothstep(0.35, 0.80, N.y) * snow;
        albedo = mix(albedo, vec3(0.88, 0.92, 0.97), snowMask * 0.90);
        roughness = mix(roughness, 0.36, snowMask);
    }
    if (wetness > 0.01 && emissive < 0.1) {
        albedo = mix(albedo, albedo * 0.62, wetness * 0.65);
        roughness = mix(roughness, 0.16, wetness * 0.80);
    }

    float viewDepth = max(-(ubo.view * vec4(inWorldPos, 1.0)).z, 0.0);
    float shadow = sampleStructureShadow(inWorldPos, N, L, viewDepth);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.001);
    float NdotH = max(dot(N, H), 0.0);
    float HdotV = max(dot(H, V), 0.0);

    vec3 F0 = vec3(0.04);
    vec3 specularBRDF = (D_GGX(NdotH, roughness) * G_Smith(NdotV, NdotL, roughness) * F_Schlick(HdotV, F0)) / max(4.0 * NdotV * NdotL, 0.001);
    vec3 kD = vec3(1.0) - F_Schlick(HdotV, F0);

    float sunIntensity = ubo.atmosphereParams.z;
    float cloudDim = smoothstep(0.65, 0.98, clamp(ubo.weatherParams.x, 0.0, 1.0));
    vec3 sunRadiance = ubo.sunColor.rgb * sunIntensity * mix(1.0, 0.45, cloudDim);
    vec3 directPBR = (kD * (albedo / PI) + specularBRDF) * sunRadiance * NdotL * shadow;

    vec3 skyAmb = ubo.skyColorZenith.rgb * ubo.skyColorZenith.a * 1.65;
    vec3 groundAmb = ubo.skyColorHorizon.rgb * 0.42 * ubo.skyColorZenith.a;
    vec3 irradiance = mix(groundAmb, skyAmb, N.y * 0.5 + 0.5) * ubo.atmosphereParams.w;

    vec3 litColor = albedo * irradiance + directPBR;

    // Warm HDR emissive glow for windows/lanterns/runes (stronger at dusk & night)
    if (emissive > 0.05) {
        float nightBoost = mix(1.8, 0.35, ubo.sunColor.a);
        litColor += inColor * emissive * nightBoost;
    }

    float fogMult = max(ubo.weatherParams2.y, 0.5);
    float waterHeight = ubo.terrainParams.x;
    float heightHaze = exp(-max(inWorldPos.y - waterHeight, 0.0) * 0.035) * (0.22 * fogMult);
    float valleyFog = heightHaze * clamp(dist / (maxDist * 0.4), 0.0, 1.0);
    float expFog = 1.0 - exp(-pow(clamp(distFactor * fogMult, 0.0, 1.0) * 1.7, 2.0));
    float edgeFade = smoothstep(0.70, 0.94, distFactor);
    float fogFactor = clamp(max(expFog, edgeFade) + valleyFog, 0.0, 1.0);

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
