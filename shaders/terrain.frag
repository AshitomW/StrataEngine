#version 450

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in float inHeight;
layout(location = 3) in float inSlope;
layout(location = 4) in vec2 inUV;
layout(location = 5) flat in uint inLOD;

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

// Fast noise and hash primitives
vec2 hash22(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return -1.0 + 2.0 * fract((p3.xx + p3.yz) * p3.zy);
}

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float gradientNoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);

    vec2 g00 = hash22(i + vec2(0.0, 0.0));
    vec2 g10 = hash22(i + vec2(1.0, 0.0));
    vec2 g01 = hash22(i + vec2(0.0, 1.0));
    vec2 g11 = hash22(i + vec2(1.0, 1.0));

    float n00 = dot(g00, f - vec2(0.0, 0.0));
    float n10 = dot(g10, f - vec2(1.0, 0.0));
    float n01 = dot(g01, f - vec2(0.0, 1.0));
    float n11 = dot(g11, f - vec2(1.0, 1.0));

    return mix(mix(n00, n10, u.x), mix(n01, n11, u.x), u.y);
}

float fbm(vec2 p, int octaves) {
    float sum = 0.0;
    float amp = 0.5;
    float freq = 1.0;
    for (int i = 0; i < octaves; ++i) {
        sum += amp * gradientNoise(p * freq);
        freq *= 2.05;
        amp *= 0.48;
    }
    return sum;
}

vec3 voronoi(vec2 x) {
    vec2 n = floor(x);
    vec2 f = fract(x);
    vec2 mg;
    float md = 8.0;
    for (int j = -1; j <= 1; ++j) {
        for (int i = -1; i <= 1; ++i) {
            vec2 g = vec2(float(i), float(j));
            vec2 o = hash22(n + g) * 0.5 + 0.5;
            vec2 r = g + o - f;
            float d = dot(r, r);
            if (d < md) {
                md = d;
                mg = g;
            }
        }
    }
    return vec3(sqrt(md), mg);
}

vec3 aces_film(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// -------------------------------------------------------------
// Procedural Materials
// -------------------------------------------------------------

vec3 sampleGrassTexture(vec2 p, out vec3 normalOffset, out float roughness) {
    // Macro variation: moisture patches & color tones
    float macroMoisture = fbm(p * 0.04, 3);
    float macroThatch   = fbm(p * 0.12 + vec2(14.2, 53.1), 3);

    // Meso variation: Voronoi bunchgrass clumps & soil crevices
    vec3 vClump = voronoi(p * 0.85);
    float clumpEdge = smoothstep(0.10, 0.45, vClump.x);

    // Micro variation: blade striations & speckle
    float blades1 = gradientNoise(p * 5.5);
    float blades2 = gradientNoise(p * 14.0 + vec2(blades1 * 0.4));
    float bladePattern = blades1 * 0.65 + blades2 * 0.35;

    // Grass Color Palette
    vec3 deepEmerald   = vec3(0.045, 0.150, 0.035);
    vec3 lushGreen     = vec3(0.095, 0.280, 0.060);
    vec3 brightClover  = vec3(0.180, 0.420, 0.085);
    vec3 sunlitTips    = vec3(0.280, 0.480, 0.110);
    vec3 goldenThatch  = vec3(0.380, 0.320, 0.140);
    vec3 richLoamSoil  = vec3(0.140, 0.095, 0.055);

    vec3 grassCol = mix(deepEmerald, lushGreen, smoothstep(-0.3, 0.3, macroMoisture));
    grassCol = mix(grassCol, brightClover, smoothstep(0.1, 0.5, macroMoisture));
    grassCol = mix(grassCol, goldenThatch, smoothstep(0.25, 0.6, macroThatch));
    grassCol = mix(grassCol, sunlitTips, clamp(bladePattern * 0.5 + 0.2, 0.0, 0.6));

    // Expose dark loam soil in crevices between grass tufts
    grassCol = mix(richLoamSoil, grassCol, clumpEdge);

    // Procedural normal perturbation (blade bumpiness)
    float d1 = gradientNoise(p * 5.5 + vec2(0.05, 0.0)) - blades1;
    float d2 = gradientNoise(p * 5.5 + vec2(0.0, 0.05)) - blades1;
    normalOffset = vec3(-d1, 0.0, -d2) * 1.5;
    roughness = 0.85;

    return grassCol;
}

vec3 sampleRockTexture(vec3 worldPos, vec3 N, out vec3 normalOffset, out float roughness) {
    vec3 absN = abs(N);
    vec3 blendWeight = pow(absN, vec3(4.0));
    blendWeight /= (blendWeight.x + blendWeight.y + blendWeight.z + 0.0001);

    // Geological strata bands
    float strata = sin(worldPos.y * 0.65 + fbm(worldPos.xz * 0.15, 3) * 3.5) * 0.5 + 0.5;

    // Rock Triplanar noise
    float nXY = fbm(worldPos.xy * 0.75, 4);
    float nYZ = fbm(worldPos.yz * 0.75, 4);
    float nXZ = fbm(worldPos.xz * 0.75, 4);
    float rockNoise = nYZ * blendWeight.x + nXZ * blendWeight.y + nXY * blendWeight.z;

    // Micro grain & chiseled fissures
    vec3 vRock = voronoi(worldPos.xz * 0.45 + worldPos.y * 0.25);
    float fissure = smoothstep(0.05, 0.35, vRock.x);

    // Rock Colors
    vec3 darkGranite  = vec3(0.11, 0.12, 0.14);
    vec3 lightGranite = vec3(0.24, 0.25, 0.28);
    vec3 rustOchre    = vec3(0.42, 0.22, 0.10);
    vec3 quartzVein   = vec3(0.55, 0.55, 0.58);
    vec3 cliffLichen  = vec3(0.22, 0.28, 0.12);

    vec3 col = mix(darkGranite, lightGranite, strata);
    col = mix(col, rustOchre, smoothstep(0.60, 0.85, rockNoise));
    col = mix(col, quartzVein, smoothstep(0.85, 0.95, rockNoise));
    col *= (0.75 + fissure * 0.35);

    // Lichen on top/gentle shelves
    float lichenMask = smoothstep(0.4, 0.7, N.y) * smoothstep(0.2, 0.6, rockNoise);
    col = mix(col, cliffLichen, lichenMask * 0.65);

    normalOffset = vec3(nXY - 0.5, 0.0, nYZ - 0.5) * 0.8;
    roughness = 0.92;
    return col;
}

vec3 sampleSoilAndBeach(vec2 p, float moisture, out vec3 normalOffset, out float roughness) {
    float nPebbles = gradientNoise(p * 8.0);
    vec3 vPebbles = voronoi(p * 2.5);
    float pebbleShape = smoothstep(0.12, 0.35, vPebbles.x);

    vec3 goldenSand   = vec3(0.64, 0.52, 0.32);
    vec3 wetSand      = vec3(0.32, 0.25, 0.16);
    vec3 riverGravel  = vec3(0.25, 0.22, 0.19);
    vec3 darkLoam     = vec3(0.16, 0.11, 0.07);

    vec3 col = mix(goldenSand, riverGravel, smoothstep(0.2, 0.7, nPebbles));
    col = mix(col, darkLoam, smoothstep(0.3, 0.8, moisture));
    col = mix(col * 0.75, col * 1.15, pebbleShape);

    normalOffset = vec3(nPebbles * 0.3, 0.0, vPebbles.x * 0.3);
    roughness = 0.80;
    return col;
}

vec3 sampleSnowTexture(vec3 worldPos, vec3 N, out vec3 normalOffset, out float roughness) {
    float sparkle = pow(hash12(worldPos.xz * 12.0), 12.0) * 1.8;
    float microWave = gradientNoise(worldPos.xz * 1.5) * 0.04;

    vec3 alpineSnow   = vec3(0.88, 0.92, 0.97);
    vec3 blueShadow   = vec3(0.65, 0.76, 0.88);

    vec3 col = mix(blueShadow, alpineSnow, clamp(N.y * 0.6 + 0.4 + microWave, 0.0, 1.0));
    col += vec3(sparkle * 0.4);

    normalOffset = vec3(microWave * 2.0);
    roughness = 0.45;
    return col;
}

void main() {
    vec3 N = normalize(inNormal);
    vec3 L = normalize(ubo.sunDir.xyz);
    vec3 D = normalize(inWorldPos - ubo.cameraPos.xyz);
    vec3 V = -D;
    vec3 H = normalize(L + V);

    float waterHeight = ubo.terrainParams.x;
    int debugMode = int(ubo.terrainParams.w);
    float amplitude = max(ubo.biomeParams.x, 15.0);
    uint preset = uint(ubo.biomeParams.y);

    if (debugMode == 1) {
        vec3 lodColors[4] = vec3[4](
            vec3(0.08, 0.85, 0.18),
            vec3(0.88, 0.75, 0.08),
            vec3(0.88, 0.38, 0.08),
            vec3(0.82, 0.08, 0.08)
        );
        vec3 col = lodColors[clamp(inLOD, 0u, 3u)];
        float ndotl = max(dot(N, L), 0.25);
        outColor = vec4(col * ndotl, 1.0);
        return;
    } else if (debugMode == 2) {
        outColor = vec4(N * 0.5 + 0.5, 1.0);
        return;
    } else if (debugMode == 3) {
        outColor = vec4(vec3(inSlope), 1.0);
        return;
    }

    float h = inHeight;
    float relH = (h - waterHeight) / amplitude;
    float slope = inSlope;

    // Sample multi-scale material layers
    vec3 grassNorm, rockNorm, sandNorm, snowNorm;
    float grassRough, rockRough, sandRough, snowRough;

    vec3 colGrass = sampleGrassTexture(inWorldPos.xz, grassNorm, grassRough);
    vec3 colRock  = sampleRockTexture(inWorldPos, N, rockNorm, rockRough);
    vec3 colSand  = sampleSoilAndBeach(inWorldPos.xz, fbm(inWorldPos.xz * 0.05, 3), sandNorm, sandRough);
    vec3 colSnow  = sampleSnowTexture(inWorldPos, N, snowNorm, snowRough);

    vec3 surfaceAlbedo;
    vec3 normalPerturb;
    float surfaceRoughness;
    float isSnow = 0.0;

    // Cliff rock blend threshold with noise breakup
    float slopeNoise = gradientNoise(inWorldPos.xz * 0.35) * 0.12;
    float cliffFactor = smoothstep(0.28, 0.55, slope + slopeNoise);

    if (preset == 0u) { // Mountains
        // Valley floor -> Grass -> Pine/Shrub -> Cliff Rock -> Alpine Snow
        vec3 ground = mix(colSand, colGrass, smoothstep(-0.02, 0.08, relH));
        ground = mix(ground, colRock, smoothstep(0.35, 0.65, relH));
        ground = mix(ground, colRock, cliffFactor);

        float snowAlt = smoothstep(0.55, 0.75, relH + slopeNoise);
        float snowSlope = 1.0 - smoothstep(0.25, 0.55, slope);
        float snowFactor = clamp(snowAlt * snowSlope * 1.2, 0.0, 1.0);

        surfaceAlbedo = mix(ground, colSnow, snowFactor);
        normalPerturb = mix(mix(grassNorm, rockNorm, cliffFactor), snowNorm, snowFactor);
        surfaceRoughness = mix(mix(grassRough, rockRough, cliffFactor), snowRough, snowFactor);
        isSnow = snowFactor;

    } else if (preset == 1u) { // Hills
        vec3 ground = mix(colSand, colGrass, smoothstep(-0.03, 0.06, relH));
        ground = mix(ground, colRock, cliffFactor * 0.85);

        surfaceAlbedo = ground;
        normalPerturb = mix(grassNorm, rockNorm, cliffFactor);
        surfaceRoughness = mix(grassRough, rockRough, cliffFactor);

    } else if (preset == 2u) { // Canyons
        vec3 ground = mix(colSand, colRock, smoothstep(0.0, 0.08, relH));
        ground = mix(ground, colGrass, smoothstep(0.70, 0.95, relH) * (1.0 - cliffFactor));
        ground = mix(ground, colRock, cliffFactor);

        surfaceAlbedo = ground;
        normalPerturb = mix(sandNorm, rockNorm, cliffFactor);
        surfaceRoughness = rockRough;

    } else if (preset == 3u) { // Islands
        vec3 beach = colSand;
        vec3 jungle = colGrass;
        vec3 basalt = colRock;

        vec3 ground = mix(beach, jungle, smoothstep(0.01, 0.10, relH));
        ground = mix(ground, basalt, smoothstep(0.50, 0.90, relH));
        ground = mix(ground, basalt, cliffFactor * 0.90);

        surfaceAlbedo = ground;
        normalPerturb = mix(mix(sandNorm, grassNorm, smoothstep(0.01, 0.10, relH)), rockNorm, cliffFactor);
        surfaceRoughness = mix(grassRough, rockRough, cliffFactor);

    } else { // Multi-Biome
        vec2 macroP = inWorldPos.xz * 0.00040;
        float continent = fbm(macroP, 3);
        float moisture  = fbm(macroP + vec2(12.3, 45.6), 3);

        float wIsland   = smoothstep(0.05, -0.20, continent);
        float wHighland = smoothstep(-0.10, 0.25, continent);
        float wHills    = 1.0 - max(wIsland, wHighland);
        float wCanyon   = wHighland * smoothstep(-0.15, 0.20, moisture);
        float wMountain = wHighland * (1.0 - smoothstep(-0.15, 0.20, moisture));

        vec3 ground = mix(colGrass, colRock, cliffFactor);
        ground = mix(colSand, ground, smoothstep(-0.02, 0.08, relH));
        surfaceAlbedo = ground;
        normalPerturb = mix(grassNorm, rockNorm, cliffFactor);
        surfaceRoughness = mix(grassRough, rockRough, cliffFactor);
    }

    // Apply micro-normal bump to mesh normal
    vec3 finalNormal = normalize(N + normalPerturb * 0.40);
    float roughness = clamp(surfaceRoughness, 0.08, 0.98);

    // Underwater and Shoreline Wetness
    if (h < waterHeight) {
        vec3 submergedSand = vec3(0.14, 0.12, 0.10);
        vec3 riverBed = vec3(0.10, 0.09, 0.08);
        vec3 underwaterAlbedo = mix(submergedSand, riverBed, smoothstep(0.0, 0.5, inSlope));
        float depth = waterHeight - h;
        surfaceAlbedo = mix(surfaceAlbedo * 0.65, underwaterAlbedo, smoothstep(0.2, 2.5, depth));
        roughness = 0.25;
    } else {
        float wetness = smoothstep(1.6, 0.0, h - waterHeight);
        surfaceAlbedo = mix(surfaceAlbedo, surfaceAlbedo * 0.58, wetness * 0.70);
        roughness = mix(roughness, 0.12, wetness * 0.85);
    }

    // Cook-Torrance PBR Direct Lighting
    float NdotL = max(dot(finalNormal, L), 0.0);
    float NdotV = max(dot(finalNormal, V), 0.001);
    float NdotH = max(dot(finalNormal, H), 0.0);
    float HdotV = max(dot(H, V), 0.0);

    vec3 F0 = mix(vec3(0.04), vec3(0.03), isSnow);
    float NDF = D_GGX(NdotH, roughness);
    float G = G_Smith(NdotV, NdotL, roughness);
    vec3 F = F_Schlick(HdotV, F0);

    vec3 numerator = NDF * G * F;
    float denominator = max(4.0 * NdotV * NdotL, 0.001);
    vec3 specularBRDF = numerator / denominator;
    vec3 kD = (vec3(1.0) - F);

    float sunIntensity = ubo.atmosphereParams.z;
    vec3 sunRadiance = ubo.sunColor.rgb * sunIntensity;
    vec3 directPBR = (kD * (surfaceAlbedo / PI) + specularBRDF) * sunRadiance * NdotL;

    // Soft wrap fill for organic grass/snow bounce
    float wrapFill = pow(clamp(dot(finalNormal, L) * 0.35 + 0.65, 0.0, 1.0), 2.0) * 0.08;
    directPBR += surfaceAlbedo * sunRadiance * wrapFill;

    // Caustics on submerged floor
    if (h < waterHeight && L.y > -0.05) {
        float timeVal = ubo.terrainParams.z;
        vec2 cUV1 = inWorldPos.xz * 0.45 + vec2(timeVal * 0.4, timeVal * 0.25);
        vec2 cUV2 = inWorldPos.xz * 0.75 - vec2(timeVal * 0.3, -timeVal * 0.5);
        float c1 = abs(sin(cUV1.x * 3.14 + sin(cUV1.y * 2.5 + timeVal)));
        float c2 = abs(cos(cUV2.x * 2.7 + cos(cUV2.y * 3.2 - timeVal)));
        float caustic = pow(c1 * c2, 1.6) * 1.8;
        float depthAtten = clamp(1.0 - (waterHeight - h) * 0.12, 0.0, 1.0);
        directPBR += surfaceAlbedo * sunRadiance * caustic * depthAtten * 0.35 * max(L.y, 0.0);
    }

    // Hemisphere Sky IBL & Ambient Occlusion
    float slopeAO = clamp(1.0 - inSlope * 0.25, 0.55, 1.0);
    float valleyAO = clamp(smoothstep(-10.0, 25.0, inHeight - waterHeight) * 0.25 + 0.75, 0.60, 1.0);
    float ao = slopeAO * valleyAO;

    float iblStrength = ubo.atmosphereParams.w;
    vec3 skyAmb = ubo.skyColorZenith.rgb * ubo.skyColorZenith.a * 1.65;
    vec3 groundAmb = ubo.skyColorHorizon.rgb * 0.42 * ubo.skyColorZenith.a;
    vec3 irradiance = mix(groundAmb, skyAmb, finalNormal.y * 0.5 + 0.5) * iblStrength;

    vec3 F_ibl = F_Schlick(NdotV, F0) * (1.0 - roughness * 0.75);
    vec3 reflDir = reflect(-V, finalNormal);
    vec3 envSpec = mix(ubo.skyColorHorizon.rgb, ubo.skyColorZenith.rgb, clamp(reflDir.y * 0.7 + 0.3, 0.0, 1.0)) * sunIntensity * 0.18;
    vec3 ambientPBR = (surfaceAlbedo * (1.0 - F_ibl) * irradiance + envSpec * F_ibl) * ao;

    vec3 litColor = ambientPBR + directPBR;

    // Atmospheric Distance Fog & Valley Mist
    float dist = length(ubo.cameraPos.xyz - inWorldPos);
    float maxDist = max(ubo.biomeParams.w, 150.0);

    float distFactor = clamp(dist / maxDist, 0.0, 1.0);
    float expFog = 1.0 - exp(-pow(distFactor * 1.7, 2.0));
    float edgeFade = smoothstep(0.70, 0.94, distFactor);
    float distFog = clamp(max(expFog, edgeFade), 0.0, 1.0);

    float heightHaze = exp(-max(inHeight - waterHeight, 0.0) * 0.035) * 0.22;
    float valleyFog = heightHaze * clamp(dist / (maxDist * 0.4), 0.0, 1.0);

    float fogFactor = clamp(distFog + valleyFog, 0.0, 1.0);

    float elevation = clamp(D.y * 0.75 + 0.25, 0.0, 1.0);
    vec3 horizonAtmosphere = mix(ubo.skyColorHorizon.rgb, ubo.skyColorZenith.rgb, elevation);

    float sunDot = max(dot(D, L), 0.0);
    float g = clamp(ubo.atmosphereParams.y, 0.1, 0.95);
    float mie = (1.0 - g * g) / pow(max(1.0 + g * g - 2.0 * g * sunDot, 1e-4), 1.5) * 0.06;
    float sunGlow = pow(sunDot, 16.0) * 0.20 + mie * 0.65;
    vec3 fogColor = horizonAtmosphere + ubo.sunColor.rgb * sunGlow * sunIntensity * 0.45 * ubo.sunColor.a;

    vec3 finalColor = mix(litColor, fogColor, fogFactor);

    // Output linear HDR (tone mapping & sRGB encoding happen in postprocess.frag)
    outColor = vec4(max(finalColor, vec3(0.0)), 1.0);
}
