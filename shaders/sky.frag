#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

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

layout(set = 2, binding = 0) uniform sampler2D uSkyViewLUT;
layout(set = 2, binding = 1) uniform sampler3D uCloudNoise3D;

const float PI = 3.14159265359;

float hash21(vec2 p) {
    p = fract(p * vec2(234.34, 435.345));
    p += dot(p, p + 34.23);
    return fract(p.x * p.y);
}

float hash31(vec3 p) {
    p = fract(p * vec3(443.897, 441.423, 437.195));
    p += dot(p, p.yzx + 19.19);
    return fract((p.x + p.y) * p.z);
}

vec2 dirToSkyViewUV(vec3 dir) {
    float azimuth = atan(dir.z, dir.x);
    float u = azimuth / (2.0 * PI) + 0.5;
    float lat = asin(clamp(dir.y, -1.0, 1.0));
    float v = 0.5 + 0.5 * sign(lat) * sqrt(abs(lat) / (PI * 0.5));
    return vec2(u, clamp(v, 0.002, 0.998));
}

float hgPhase(float cosTheta, float g) {
    float g2 = g * g;
    float denom = max(1.0 + g2 - 2.0 * g * cosTheta, 1e-4);
    return (1.0 - g2) / (4.0 * PI * denom * sqrt(denom));
}

float sampleCloudDensity(vec3 p, float coverage, float time, float windSpeed) {
    float hFrac = clamp((p.y - 950.0) / 1400.0, 0.0, 1.0);
    float heightMask = smoothstep(0.0, 0.18, hFrac) * smoothstep(1.0, 0.45, hFrac);
    if (heightMask <= 0.001) return 0.0;

    vec3 windOffset = vec3(time * windSpeed * 1.4, 0.0, time * windSpeed * 0.7);
    vec3 uvw = (p + windOffset) * 0.00028;
    vec4 base = textureLod(uCloudNoise3D, uvw, 0.0);

    float shape = base.r * 0.72 + base.g * 0.28;
    float baseCloud = clamp(shape - (1.0 - coverage * 0.92), 0.0, 1.0) / max(coverage * 0.92, 0.08);
    baseCloud *= heightMask;
    if (baseCloud <= 0.001) return 0.0;

    float detail = textureLod(uCloudNoise3D, uvw * 2.7 + vec3(0.15), 0.0).b;
    float eroded = clamp(baseCloud - (1.0 - detail) * 0.28 * (1.0 - hFrac * 0.4), 0.0, 1.0);
    return eroded * 0.14;
}

void main() {
    vec2 ndc = inUV * 2.0 - 1.0;
    vec4 wp = ubo.invViewProj * vec4(ndc.x, ndc.y, 1.0, 1.0);
    vec3 rayDir = normalize(wp.xyz / max(wp.w, 1e-5) - ubo.cameraPos.xyz);

    float time = ubo.terrainParams.z;
    float sinElev = ubo.sunDir.w;
    float dayFactor = ubo.sunColor.a;
    float starFactor = ubo.skyColorHorizon.a;
    float sunIntensity = ubo.atmosphereParams.z;

    float coverage = clamp(ubo.weatherParams.x, 0.0, 1.0);
    float windSpeed = max(ubo.weatherParams2.x, 1.0);
    float lightning = ubo.weatherParams2.z;

    vec3 skyColor = texture(uSkyViewLUT, dirToSkyViewUV(rayDir)).rgb;

    // Darken sky slightly under heavy storm overcast
    float stormDim = smoothstep(0.60, 0.98, coverage);
    vec3 stormTint = mix(vec3(0.06, 0.07, 0.09), vec3(0.28, 0.30, 0.34), dayFactor);
    skyColor = mix(skyColor, stormTint, stormDim * 0.65);

    float sunAngle = ((ubo.cameraPos.w - 6.0) / 12.0) * PI;
    float cosElev = cos(sunAngle);
    vec3 trueSunDir = normalize(vec3(cosElev * 0.80, sinElev, 0.35));
    vec3 trueMoonDir = normalize(vec3(-cosElev * 0.80, -sinElev, -0.35));

    // Physical HDR Sun Disc
    float sunVisibility = smoothstep(-0.04, 0.06, sinElev) * (1.0 - stormDim * 0.85);
    if (sunVisibility > 0.001) {
        float sunDot = max(dot(rayDir, trueSunDir), 0.0);
        float sunDisc = smoothstep(0.99945, 0.99975, sunDot) * (14.0 * sunIntensity);
        float sunCorona = pow(sunDot, 48.0) * (1.2 * sunIntensity);
        skyColor += ubo.sunColor.rgb * (sunDisc + sunCorona) * sunVisibility;
    }

    // Moon Disc
    float moonVisibility = smoothstep(-0.04, 0.08, -sinElev) * (1.0 - stormDim * 0.85);
    if (moonVisibility > 0.001) {
        float moonDot = max(dot(rayDir, trueMoonDir), 0.0);
        float moonDisc = smoothstep(0.9995, 0.9998, moonDot) * 2.5;
        float moonGlow = pow(moonDot, 36.0) * 0.35;
        skyColor += vec3(0.75, 0.86, 1.0) * (moonDisc + moonGlow) * moonVisibility;
    }

    // Twinkling stars at night
    if (starFactor > 0.01 && rayDir.y > 0.03 && stormDim < 0.7) {
        vec3 starP = rayDir * 170.0;
        vec3 starCell = floor(starP);
        vec3 starFrac = fract(starP) - 0.5;
        float starRnd = hash31(starCell);
        float starDist = length(starFrac);
        float twinkle = sin(time * 3.0 + starRnd * 40.0) * 0.35 + 0.65;
        float starVal = smoothstep(0.11, 0.02, starDist) * step(0.965, starRnd) * twinkle * 2.5;
        float horizonFade = smoothstep(0.03, 0.25, rayDir.y);
        skyColor += vec3(starVal * horizonFade * starFactor * (1.0 - stormDim));
    }

    // 3D Volumetric Raymarched Clouds
    if (rayDir.y > 0.015 && coverage > 0.02) {
        float tMin = (950.0 - ubo.cameraPos.y) / rayDir.y;
        float tMax = (2350.0 - ubo.cameraPos.y) / rayDir.y;
        tMin = max(tMin, 100.0);
        tMax = min(tMax, 14000.0);

        if (tMax > tMin) {
            const int STEPS = 16;
            float dt = (tMax - tMin) / float(STEPS);
            float jitter = hash21(gl_FragCoord.xy + fract(time) * 100.0);

            vec3 skyZenithSample = texture(uSkyViewLUT, dirToSkyViewUV(vec3(0.0, 0.9, 0.1))).rgb;
            vec3 sunDir = normalize(ubo.sunDir.xyz);
            float cosTheta = dot(rayDir, sunDir);
            float phase = mix(hgPhase(cosTheta, 0.76), hgPhase(cosTheta, -0.25), 0.35);

            float transmittance = 1.0;
            vec3 cloudLight = vec3(0.0);

            for (int i = 0; i < STEPS; ++i) {
                float t = tMin + (float(i) + jitter) * dt;
                vec3 p = ubo.cameraPos.xyz + rayDir * t;
                float dens = sampleCloudDensity(p, coverage, time, windSpeed);

                if (dens > 0.0005) {
                    float shadowDens = sampleCloudDensity(p + sunDir * 220.0, coverage, time, windSpeed);
                    float sunAtten = exp(-shadowDens * 8.0);
                    float powder = 1.0 - exp(-dens * dt * 0.025);

                    vec3 directCol = ubo.sunColor.rgb * sunIntensity * (0.45 + phase * 3.5) * sunAtten * mix(1.0, 0.38, stormDim);
                    vec3 ambCol = skyZenithSample * (1.2 - stormDim * 0.55) + vec3(0.025, 0.035, 0.055) * (1.0 - dayFactor);
                    vec3 flashCol = vec3(0.75, 0.85, 1.0) * lightning * 6.0;

                    vec3 stepLight = (directCol + ambCol + flashCol) * mix(0.65, 1.25, powder);
                    float stepTrans = exp(-dens * dt * 0.016);
                    cloudLight += transmittance * stepLight * (1.0 - stepTrans);
                    transmittance *= stepTrans;

                    if (transmittance < 0.03) break;
                }
            }

            float horizonFade = smoothstep(0.015, 0.16, rayDir.y);
            vec3 horizonFog = texture(uSkyViewLUT, dirToSkyViewUV(normalize(vec3(rayDir.x, 0.04, rayDir.z)))).rgb;
            vec3 blendedCloud = mix(horizonFog, cloudLight + skyColor * transmittance, horizonFade);
            skyColor = mix(skyColor, blendedCloud, horizonFade);
        }
    }

    // Lightning sky flash
    skyColor += vec3(0.55, 0.68, 0.95) * lightning * 1.4;

    outColor = vec4(max(skyColor, vec3(0.0)), 1.0);
}
