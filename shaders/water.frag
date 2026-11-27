#version 450

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in float inWaveHeight;
layout(location = 3) in float inFoamFactor;
layout(location = 4) in vec3 inUndisplacedPos;

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

layout(set = 2, binding = 0) uniform sampler2D uWaveDispFoam;
layout(set = 2, binding = 1) uniform sampler2D uRipples;
layout(set = 2, binding = 2) uniform sampler2D uWaveNormalSlope;
layout(set = 2, binding = 3) uniform sampler2D uShadowAtlas;
layout(set = 2, binding = 4) uniform sampler2D uSkyViewLUT;
layout(set = 2, binding = 5) uniform sampler2D uOpaqueColor;
layout(set = 2, binding = 6) uniform sampler2D uOpaqueDepth;

layout(push_constant) uniform WaterPC {
    vec4 waterParams1; // x: waterHeight, y: waveAmplitude, z: waveSpeed, w: time
    vec4 waterParams2; // x: clarity, y: windSpeed, z: choppiness, w: debugMode
    vec4 gridCenter;   // x: centerX, y: centerZ, z: rippleCenterX, w: rippleCenterZ
} pc;

const float PI = 3.14159265359;

vec2 dirToSkyViewUV(vec3 dir) {
    float azimuth = atan(dir.z, dir.x);
    float u = azimuth / (2.0 * PI) + 0.5;
    float lat = asin(clamp(dir.y, -1.0, 1.0));
    float v = 0.5 + 0.5 * sign(lat) * sqrt(abs(lat) / (PI * 0.5));
    return vec2(u, clamp(v, 0.002, 0.998));
}

vec3 reconstructWorldPos(vec2 screenUV, float depth) {
    vec2 ndc = screenUV * 2.0 - 1.0;
    vec4 worldPos = inverse(ubo.viewProj) * vec4(ndc, depth, 1.0);
    return worldPos.xyz / max(worldPos.w, 1e-5);
}

float sampleWaterShadow(vec3 worldPos, float viewDepth) {
    float strength = ubo.shadowParams.x;
    if (strength <= 0.001) return 1.0;

    int cascade = 3;
    if (viewDepth < ubo.cascadeSplits.x) cascade = 0;
    else if (viewDepth < ubo.cascadeSplits.y) cascade = 1;
    else if (viewDepth < ubo.cascadeSplits.z) cascade = 2;

    vec4 lsPos = ubo.lightSpaceMatrices[cascade] * vec4(worldPos, 1.0);
    vec3 proj = lsPos.xyz / lsPos.w;
    vec2 tileUV = proj.xy * 0.5 + 0.5;
    if (any(lessThan(tileUV, vec2(0.01))) || any(greaterThan(tileUV, vec2(0.99))) || proj.z > 1.0) {
        return 1.0;
    }

    vec2 tileOffset = vec2(float(cascade % 2), float(cascade / 2)) * 0.5;
    vec2 atlasUV = tileOffset + tileUV * 0.5;
    float bias = ubo.shadowParams.y * float(cascade + 1);
    float d = texture(uShadowAtlas, atlasUV).r;
    float shadow = ((proj.z - bias) <= d) ? 1.0 : 0.0;
    float distFade = 1.0 - smoothstep(ubo.cascadeSplits.w * 0.80, ubo.cascadeSplits.w, viewDepth);
    return mix(1.0, shadow, strength * distFade);
}

void main() {
    float time = pc.waterParams1.w;
    float waveSpeed = pc.waterParams1.z;
    float clarity = max(pc.waterParams2.x, 0.04);

    vec3 V = normalize(ubo.cameraPos.xyz - inWorldPos);
    vec3 L = normalize(ubo.sunDir.xyz);
    float dist = length(ubo.cameraPos.xyz - inWorldPos);

    // Sample GPU spectral wave normals at 3 scales (swell + chop + capillary)
    vec2 uv1 = inUndisplacedPos.xz / 64.0;
    vec2 uv2 = inUndisplacedPos.xz / 23.0 + vec2(0.37, 0.71);
    vec2 uv3 = inUndisplacedPos.xz / 6.5 + vec2(time * 0.08 * waveSpeed, -time * 0.06 * waveSpeed);

    vec3 n1 = texture(uWaveNormalSlope, uv1).xyz;
    vec3 n2 = texture(uWaveNormalSlope, uv2).xyz;
    vec3 n3 = texture(uWaveNormalSlope, uv3).xyz;

    float nearDetail = 1.0 - smoothstep(40.0, 320.0, dist);
    vec3 N = normalize(n1 * 0.60 + n2 * 0.30 + vec3(n3.x, 1.0, n3.z) * (0.20 * nearDetail));

    // Add interactive ripple normal gradient near camera
    vec2 ripUV = (inUndisplacedPos.xz - pc.gridCenter.zw) / 48.0 + 0.5;
    if (all(greaterThan(ripUV, vec2(0.02))) && all(lessThan(ripUV, vec2(0.98)))) {
        vec4 rip = texture(uRipples, ripUV);
        N = normalize(N + vec3(-rip.z * 2.5, 0.0, -rip.w * 2.5));
    }

    // Exact seabed depth & refraction via copied opaque depth/color buffers
    vec2 screenUV = gl_FragCoord.xy / vec2(textureSize(uOpaqueColor, 0));
    float sceneDepth = texture(uOpaqueDepth, screenUV).r;
    vec3 seabedPos = reconstructWorldPos(screenUV, sceneDepth);
    float waterDepth = (sceneDepth >= 0.9999) ? 80.0 : max(inWorldPos.y - seabedPos.y, 0.0);

    // Screen-space refraction offset scaled by depth and surface normal
    vec2 refractOffset = N.xz * clamp(waterDepth * 0.015, 0.0, 0.045);
    vec2 refrUV = clamp(screenUV + refractOffset, vec2(0.002), vec2(0.998));
    float refrDepth = texture(uOpaqueDepth, refrUV).r;
    vec3 refrSeabedPos = reconstructWorldPos(refrUV, refrDepth);
    float refrWaterDepth = (refrDepth >= 0.9999) ? 80.0 : max(inWorldPos.y - refrSeabedPos.y, 0.0);

    // Reject refraction sample if it hit above-water geometry
    if (refrSeabedPos.y > inWorldPos.y && refrDepth < 0.9999) {
        refrUV = screenUV;
        refrWaterDepth = waterDepth;
        refrSeabedPos = seabedPos;
    }

    vec3 refractedScene = texture(uOpaqueColor, refrUV).rgb;

    // Fresnel (Schlick)
    vec3 faceN = (dot(N, V) < 0.0) ? -N : N;
    float NdotV = max(dot(faceN, V), 0.001);
    float R0 = 0.0204;
    float fresnel = clamp(R0 + (1.0 - R0) * pow(1.0 - NdotV, 5.0), 0.03, 0.96);

    // Physical Sky-View LUT reflection + Sun specular glint
    float sunIntensity = ubo.atmosphereParams.z;
    vec3 R = reflect(-V, faceN);
    vec3 reflDir = vec3(R.x, max(R.y, 0.01), R.z);
    vec3 skyRefl = texture(uSkyViewLUT, dirToSkyViewUV(reflDir)).rgb;
    float sunReflDot = max(dot(R, L), 0.0);
    float sunGlint = pow(sunReflDot, 380.0) * 8.0 + pow(sunReflDot, 42.0) * 0.55;
    vec3 reflectionColor = skyRefl + ubo.sunColor.rgb * sunGlint * sunIntensity * 0.75;

    // Shadows & illumination
    float viewDepth = max(-(ubo.view * vec4(inWorldPos, 1.0)).z, 0.0);
    float shadow = sampleWaterShadow(inWorldPos, viewDepth);

    vec3 skyAmb = ubo.skyColorZenith.rgb * ubo.skyColorZenith.a * 1.4;
    vec3 groundAmb = ubo.skyColorHorizon.rgb * 0.35 * ubo.skyColorZenith.a;
    vec3 ambient = mix(groundAmb, skyAmb, faceN.y * 0.5 + 0.5) * ubo.atmosphereParams.w;
    float NdotL = max(dot(faceN, L), 0.0);
    vec3 directLight = ubo.sunColor.rgb * sunIntensity * 0.65 * NdotL * shadow;

    // Beer-Lambert spectral absorption through water volume
    vec3 sigma = vec3(0.42, 0.14, 0.06) / clarity;
    vec3 transmittance = exp(-refrWaterDepth * sigma);

    // Projected animated caustics on refracted seabed
    if (refrWaterDepth < 18.0 && refrDepth < 0.9999 && L.y > 0.02) {
        vec2 cUV1 = refrSeabedPos.xz * 0.55 + vec2(time * 0.4, time * 0.25);
        vec2 cUV2 = refrSeabedPos.xz * 0.85 - vec2(time * 0.3, -time * 0.5);
        float c1 = abs(sin(cUV1.x * 3.14 + sin(cUV1.y * 2.5 + time)));
        float c2 = abs(cos(cUV2.x * 2.7 + cos(cUV2.y * 3.2 - time)));
        float caustic = pow(c1 * c2, 1.8) * 1.6;
        float cFade = exp(-refrWaterDepth * 0.14);
        refractedScene += ubo.sunColor.rgb * sunIntensity * caustic * cFade * 0.28 * shadow;
    }

    vec3 scatterTint = vec3(0.015, 0.16, 0.22) * (ambient + directLight * 0.5);
    vec3 waterBodyColor = mix(scatterTint, refractedScene * transmittance, clamp(transmittance.b, 0.0, 1.0));

    // Subsurface scattering through wave crests facing the sun
    float crestMask = clamp(inWaveHeight * 1.8 + 0.25, 0.0, 1.0);
    float backLit = pow(max(dot(V, -L + faceN * 0.45), 0.0), 4.0);
    vec3 sssGlow = vec3(0.04, 0.42, 0.38) * ubo.sunColor.rgb * sunIntensity * backLit * crestMask * 0.75 * shadow;
    waterBodyColor += sssGlow;

    // Combine refraction/volume + sky reflection + HDR sun specular
    vec3 H = normalize(L + V);
    float NdotH = max(dot(faceN, H), 0.0);
    float specular = pow(NdotH, 420.0) * 6.0 + pow(NdotH, 52.0) * 0.45;
    vec3 waterColor = mix(waterBodyColor, reflectionColor, fresnel);
    if (L.y > -0.02) {
        waterColor += ubo.sunColor.rgb * specular * sunIntensity * 0.75 * shadow;
    }

    // Shoreline foam & wave-crest whitecaps
    float foamPattern = texture(uWaveDispFoam, inUndisplacedPos.xz * 0.18 + vec2(time * 0.04, -time * 0.03)).w;
    float shoreFoam = smoothstep(0.65, 0.04, refrWaterDepth) * smoothstep(0.20, 0.75, foamPattern + 0.25) * 0.75;
    float crestFoam = smoothstep(0.55, 0.92, inFoamFactor) * smoothstep(0.25, 0.80, foamPattern + 0.15) * 0.65;
    float totalFoam = clamp(shoreFoam + crestFoam, 0.0, 0.85);

    vec3 foamColor = vec3(0.92, 0.96, 1.0) * (ambient + directLight * 0.85);
    waterColor = mix(waterColor, foamColor, totalFoam);

    // Atmospheric distance fog
    float maxDist = max(ubo.biomeParams.w, 150.0);
    float distFactor = clamp(dist / maxDist, 0.0, 1.0);
    float expFog = 1.0 - exp(-pow(distFactor * 1.7, 2.0));
    float edgeFade = smoothstep(0.70, 0.94, distFactor);
    float fogFactor = clamp(max(expFog, edgeFade), 0.0, 1.0);

    vec3 D = -V;
    float elevation = clamp(D.y * 0.75 + 0.25, 0.0, 1.0);
    vec3 horizonAtmosphere = mix(ubo.skyColorHorizon.rgb, ubo.skyColorZenith.rgb, elevation);
    float sunDot = max(dot(D, L), 0.0);
    float g = clamp(ubo.atmosphereParams.y, 0.1, 0.95);
    float mie = (1.0 - g * g) / pow(max(1.0 + g * g - 2.0 * g * sunDot, 1e-4), 1.5) * 0.06;
    float sunGlow = pow(sunDot, 16.0) * 0.20 + mie * 0.65;
    vec3 fogColor = horizonAtmosphere + ubo.sunColor.rgb * sunGlow * sunIntensity * 0.45 * ubo.sunColor.a;

    vec3 finalColor = mix(waterColor, fogColor, fogFactor);

    // Soft shoreline alpha fade eliminates hard polygon clipping edge
    float shoreAlpha = smoothstep(0.0, 0.16, waterDepth);
    float alpha = mix(shoreAlpha, 1.0, fogFactor);
    outColor = vec4(max(finalColor, vec3(0.0)), alpha);
}
