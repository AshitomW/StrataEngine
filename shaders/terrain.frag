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
    mat4 lightSpaceMatrices[4];
    vec4 cascadeSplits;
    vec4 shadowParams;
} ubo;

layout(set = 2, binding = 0) uniform sampler2D uShadowAtlas;
layout(set = 2, binding = 1) uniform sampler2DArray uMaterialAlbedo;
layout(set = 2, binding = 2) uniform sampler2DArray uMaterialNormalORM;

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

    float n00 = dot(hash22(i + vec2(0.0, 0.0)), f - vec2(0.0, 0.0));
    float n10 = dot(hash22(i + vec2(1.0, 0.0)), f - vec2(1.0, 0.0));
    float n01 = dot(hash22(i + vec2(0.0, 1.0)), f - vec2(0.0, 1.0));
    float n11 = dot(hash22(i + vec2(1.0, 1.0)), f - vec2(1.0, 1.0));

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

// Stochastic non-repeating UV tiling (2-tap rotated blend with noise mask)
void sampleStochasticLayer(float layer, vec2 uv, out vec4 albedoH, out vec4 normORM) {
    vec2 cell = floor(uv * 0.5);
    float rnd = hash12(cell);
    vec2 off = hash22(cell) * 0.5;
    float ang = rnd * 6.2831853;
    float c = cos(ang), s = sin(ang);
    vec2 uvA = mat2(c, -s, s, c) * (uv - cell * 2.0) + off;
    vec2 uvB = uv * 0.73 + vec2(0.37, 0.61);

    float mask = smoothstep(-0.25, 0.25, gradientNoise(uv * 0.45));
    vec4 a1 = texture(uMaterialAlbedo, vec3(uvA, layer));
    vec4 a2 = texture(uMaterialAlbedo, vec3(uvB, layer));
    vec4 n1 = texture(uMaterialNormalORM, vec3(uvA, layer));
    vec4 n2 = texture(uMaterialNormalORM, vec3(uvB, layer));

    albedoH = mix(a1, a2, mask);
    normORM = mix(n1, n2, mask);
}

// Triplanar projection for steep cliffs
void sampleTriplanarLayer(float layer, vec3 worldPos, vec3 N, out vec4 albedoH, out vec4 normORM) {
    vec3 w = pow(abs(N), vec3(4.0));
    w /= max(w.x + w.y + w.z, 1e-4);

    vec2 uvX = worldPos.zy * 0.085;
    vec2 uvY = worldPos.xz * 0.085;
    vec2 uvZ = worldPos.xy * 0.085;

    vec4 ax = texture(uMaterialAlbedo, vec3(uvX, layer));
    vec4 ay = texture(uMaterialAlbedo, vec3(uvY, layer));
    vec4 az = texture(uMaterialAlbedo, vec3(uvZ, layer));
    albedoH = ax * w.x + ay * w.y + az * w.z;

    vec4 nx = texture(uMaterialNormalORM, vec3(uvX, layer));
    vec4 ny = texture(uMaterialNormalORM, vec3(uvY, layer));
    vec4 nz = texture(uMaterialNormalORM, vec3(uvZ, layer));
    normORM = nx * w.x + ny * w.y + nz * w.z;
}

float sampleTerrainShadow(vec3 worldPos, vec3 N, vec3 L, float viewDepth) {
    float strength = ubo.shadowParams.x;
    if (strength <= 0.001) return 1.0;

    int cascade = 3;
    if (viewDepth < ubo.cascadeSplits.x) cascade = 0;
    else if (viewDepth < ubo.cascadeSplits.y) cascade = 1;
    else if (viewDepth < ubo.cascadeSplits.z) cascade = 2;

    float normalOffset = 0.08 * float(cascade + 1) * (1.0 - clamp(dot(N, L), 0.0, 1.0) * 0.5);
    vec4 lsPos = ubo.lightSpaceMatrices[cascade] * vec4(worldPos + N * normalOffset, 1.0);
    vec3 proj = lsPos.xyz / lsPos.w;
    vec2 tileUV = proj.xy * 0.5 + 0.5;
    if (any(lessThan(tileUV, vec2(0.01))) || any(greaterThan(tileUV, vec2(0.99))) || proj.z > 1.0) {
        return 1.0;
    }

    vec2 tileOffset = vec2(float(cascade % 2), float(cascade / 2)) * 0.5;
    vec2 atlasUV = tileOffset + tileUV * 0.5;
    float bias = ubo.shadowParams.y * float(cascade + 1) * (1.0 + (1.0 - max(dot(N, L), 0.0)) * 1.5);

    vec2 texel = vec2(1.0 / 4096.0);
    float shadow = 0.0;

    if (cascade <= 1) {
        vec2 poisson[8] = vec2[8](
            vec2(-0.707, -0.707), vec2(0.707, -0.707), vec2(-0.707, 0.707), vec2(0.707, 0.707),
            vec2(-0.95, 0.15),   vec2(0.95, -0.15),  vec2(0.15, 0.95),   vec2(-0.15, -0.95)
        );
        float angle = hash12(floor(worldPos.xz * 8.0)) * 6.2831853;
        float ca = cos(angle), sa = sin(angle);
        mat2 rot = mat2(ca, -sa, sa, ca);
        float spread = (cascade == 0) ? 1.8 : 2.2;
        for (int i = 0; i < 8; ++i) {
            vec2 suv = atlasUV + (rot * poisson[i]) * texel * spread;
            float d = texture(uShadowAtlas, suv).r;
            shadow += ((proj.z - bias) <= d) ? 1.0 : 0.0;
        }
        shadow *= 0.125;
    } else {
        vec2 offs[4] = vec2[4](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, 1.0));
        for (int i = 0; i < 4; ++i) {
            float d = texture(uShadowAtlas, atlasUV + offs[i] * texel * 1.5).r;
            shadow += ((proj.z - bias) <= d) ? 1.0 : 0.0;
        }
        shadow *= 0.25;
    }

    float distFade = 1.0 - smoothstep(ubo.cascadeSplits.w * 0.80, ubo.cascadeSplits.w, viewDepth);
    return mix(1.0, shadow, strength * distFade);
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

    float slopeNoise = gradientNoise(inWorldPos.xz * 0.35) * 0.12;
    float cliffFactor = smoothstep(0.28, 0.55, slope + slopeNoise);

    // Compute normalized biome/material layer weights before texture fetches (early-out friendly)
    // Layers: 0=Grass, 1=Granite, 2=Sand, 3=Snow, 4=CanyonSandstone, 5=Basalt/Moss
    float w0 = 0.0, w1 = 0.0, w2 = 0.0, w3 = 0.0, w4 = 0.0, w5 = 0.0;
    float isSnow = 0.0;

    if (preset == 0u) { // Mountains
        float sandToGrass = smoothstep(-0.02, 0.08, relH);
        float grassToRock = smoothstep(0.35, 0.65, relH);
        w2 = 1.0 - sandToGrass;
        w0 = sandToGrass * (1.0 - grassToRock) * (1.0 - cliffFactor);
        w1 = clamp(grassToRock + cliffFactor, 0.0, 1.0) * (1.0 - w2 * 0.5);
        float snowAlt = smoothstep(0.55, 0.75, relH + slopeNoise);
        float snowSlope = 1.0 - smoothstep(0.25, 0.55, slope);
        w3 = clamp(snowAlt * snowSlope * 1.2, 0.0, 1.0);
        w0 *= (1.0 - w3);
        w1 *= (1.0 - w3);
        w2 *= (1.0 - w3);
        isSnow = w3;
    } else if (preset == 1u) { // Hills
        float sandToGrass = smoothstep(-0.03, 0.06, relH);
        w2 = 1.0 - sandToGrass;
        w0 = sandToGrass * (1.0 - cliffFactor * 0.85);
        w1 = sandToGrass * cliffFactor * 0.85;
    } else if (preset == 2u) { // Canyons
        float sandToRock = smoothstep(0.0, 0.08, relH);
        float topGrass = smoothstep(0.70, 0.95, relH) * (1.0 - cliffFactor);
        w2 = 1.0 - sandToRock;
        w0 = topGrass;
        w4 = sandToRock * (1.0 - topGrass);
    } else if (preset == 3u) { // Islands
        float beachToJungle = smoothstep(0.01, 0.10, relH);
        float basaltAlt = smoothstep(0.50, 0.90, relH);
        float basaltMask = clamp(basaltAlt + cliffFactor * 0.90, 0.0, 1.0);
        w2 = 1.0 - beachToJungle;
        w0 = beachToJungle * (1.0 - basaltMask);
        w5 = beachToJungle * basaltMask;
    } else { // Multi-Biome
        vec2 macroP = inWorldPos.xz * 0.00040;
        float continent = fbm(macroP, 3);
        float moisture  = fbm(macroP + vec2(12.3, 45.6), 3);
        float wIsland   = smoothstep(0.05, -0.20, continent);
        float wHighland = smoothstep(-0.10, 0.25, continent);
        float wCanyon   = wHighland * smoothstep(-0.15, 0.20, moisture);

        float sandMask = 1.0 - smoothstep(-0.02, 0.08, relH);
        w2 = sandMask;
        w0 = (1.0 - sandMask) * (1.0 - cliffFactor) * (1.0 - wCanyon * 0.7);
        w1 = (1.0 - sandMask) * cliffFactor * (1.0 - wCanyon);
        w4 = (1.0 - sandMask) * wCanyon * 0.85;
        w5 = (1.0 - sandMask) * wIsland * cliffFactor * 0.5;
    }

    vec3 surfaceAlbedo = vec3(0.0);
    vec3 tangentNorm = vec3(0.0, 0.0, 1.0);
    float surfaceRoughness = 0.85;
    float bakedAO = 1.0;

    if (ubo.shadowParams.w > 0.5) {
        vec2 uv = inWorldPos.xz * 0.11;
        float weights[6] = float[6](w0, w1, w2, w3, w4, w5);
        vec4 albedos[6];
        vec4 norms[6];
        float maxH = -1.0;

        for (int i = 0; i < 6; ++i) {
            if (weights[i] > 0.01) {
                if (slope > 0.35 && (i == 1 || i == 4 || i == 5)) {
                    sampleTriplanarLayer(float(i), inWorldPos, N, albedos[i], norms[i]);
                } else {
                    sampleStochasticLayer(float(i), uv, albedos[i], norms[i]);
                }
                maxH = max(maxH, albedos[i].a + weights[i]);
            } else {
                albedos[i] = vec4(0.0);
                norms[i] = vec4(0.5, 0.5, 0.85, 1.0);
            }
        }

        // Height-based splat blending
        float blendDepth = 0.22;
        float hThreshold = maxH - blendDepth;
        float sumW = 0.0;
        vec3 accAlbedo = vec3(0.0);
        vec2 accNormXY = vec2(0.0);
        float accRough = 0.0;
        float accAO = 0.0;

        for (int i = 0; i < 6; ++i) {
            if (weights[i] > 0.01) {
                float hw = max(albedos[i].a + weights[i] - hThreshold, 0.0);
                hw = hw * weights[i];
                sumW += hw;
                accAlbedo += albedos[i].rgb * hw;
                accNormXY += (norms[i].xy * 2.0 - 1.0) * hw;
                accRough += norms[i].z * hw;
                accAO += norms[i].w * hw;
            }
        }

        float invW = 1.0 / max(sumW, 1e-4);
        surfaceAlbedo = accAlbedo * invW;
        tangentNorm = normalize(vec3(accNormXY * invW * 0.65, 1.0));
        surfaceRoughness = accRough * invW;
        bakedAO = accAO * invW;
    } else {
        // Fallback flat-ish procedural palette when PBR texture toggle is off
        vec3 colGrass = vec3(0.11, 0.29, 0.06);
        vec3 colRock  = vec3(0.20, 0.21, 0.23);
        vec3 colSand  = vec3(0.58, 0.47, 0.29);
        vec3 colSnow  = vec3(0.88, 0.92, 0.97);
        vec3 colClay  = vec3(0.50, 0.22, 0.11);
        vec3 colBasalt = vec3(0.10, 0.11, 0.12);
        float wSum = max(w0 + w1 + w2 + w3 + w4 + w5, 1e-4);
        surfaceAlbedo = (colGrass * w0 + colRock * w1 + colSand * w2 + colSnow * w3 + colClay * w4 + colBasalt * w5) / wSum;
        surfaceRoughness = mix(0.85, 0.40, isSnow);
    }

    // Transform tangent normal to world space (RNM-style perturbation on terrain heightfield normal)
    vec3 upRef = (abs(N.y) < 0.99) ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 T = normalize(cross(upRef, N));
    vec3 B = cross(N, T);
    vec3 finalNormal = normalize(T * tangentNorm.x + B * tangentNorm.y + N * tangentNorm.z);
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

    // Cook-Torrance PBR Direct Lighting + 4-Cascade CSM Shadow
    float viewDepth = max(-(ubo.view * vec4(inWorldPos, 1.0)).z, 0.0);
    float shadow = sampleTerrainShadow(inWorldPos, N, L, viewDepth);

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
    vec3 directPBR = (kD * (surfaceAlbedo / PI) + specularBRDF) * sunRadiance * NdotL * shadow;

    // Soft wrap fill
    float wrapFill = pow(clamp(dot(finalNormal, L) * 0.35 + 0.65, 0.0, 1.0), 2.0) * 0.06;
    directPBR += surfaceAlbedo * sunRadiance * wrapFill * mix(0.25, 1.0, shadow);

    // Caustics on submerged floor
    if (h < waterHeight && L.y > -0.05) {
        float timeVal = ubo.terrainParams.z;
        vec2 cUV1 = inWorldPos.xz * 0.45 + vec2(timeVal * 0.4, timeVal * 0.25);
        vec2 cUV2 = inWorldPos.xz * 0.75 - vec2(timeVal * 0.3, -timeVal * 0.5);
        float c1 = abs(sin(cUV1.x * 3.14 + sin(cUV1.y * 2.5 + timeVal)));
        float c2 = abs(cos(cUV2.x * 2.7 + cos(cUV2.y * 3.2 - timeVal)));
        float caustic = pow(c1 * c2, 1.6) * 1.8;
        float depthAtten = clamp(1.0 - (waterHeight - h) * 0.12, 0.0, 1.0);
        directPBR += surfaceAlbedo * sunRadiance * caustic * depthAtten * 0.35 * max(L.y, 0.0) * shadow;
    }

    // Hemisphere Sky IBL & Ambient Occlusion
    float slopeAO = clamp(1.0 - inSlope * 0.25, 0.55, 1.0);
    float valleyAO = clamp(smoothstep(-10.0, 25.0, inHeight - waterHeight) * 0.25 + 0.75, 0.60, 1.0);
    float ao = slopeAO * valleyAO * bakedAO;

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
    outColor = vec4(max(finalColor, vec3(0.0)), 1.0);
}
