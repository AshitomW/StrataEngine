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
} ubo;

const float PI = 3.14159265359;

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

    vec3 directLight = sunRadiance * ((NdotL * 0.85 + wrapLight) / PI + sss / PI) + sunRadiance * spec;

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
