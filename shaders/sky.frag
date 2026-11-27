#version 450

layout(location = 0) in vec2 inUV;
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

layout(set = 2, binding = 0) uniform sampler2D uSkyViewLUT;

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

float noise2D(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(
        mix(hash21(i + vec2(0.0, 0.0)), hash21(i + vec2(1.0, 0.0)), f.x),
        mix(hash21(i + vec2(0.0, 1.0)), hash21(i + vec2(1.0, 1.0)), f.x),
        f.y
    );
}

float fbmCloud(vec2 p) {
    float sum = 0.0;
    float amp = 0.5;
    float freq = 1.0;
    for (int i = 0; i < 5; ++i) {
        sum += amp * noise2D(p * freq);
        freq *= 2.15;
        amp *= 0.48;
    }
    return sum;
}

vec2 dirToSkyViewUV(vec3 dir) {
    float azimuth = atan(dir.z, dir.x);
    float u = azimuth / (2.0 * PI) + 0.5;
    float lat = asin(clamp(dir.y, -1.0, 1.0));
    float v = 0.5 + 0.5 * sign(lat) * sqrt(abs(lat) / (PI * 0.5));
    return vec2(u, clamp(v, 0.002, 0.998));
}

void main() {
    vec2 ndc = inUV * 2.0 - 1.0;
    vec4 target = inverse(ubo.proj) * vec4(ndc.x, ndc.y, 1.0, 1.0);
    vec3 rayDir = normalize((inverse(ubo.view) * vec4(target.xyz / target.w, 0.0)).xyz);

    float time = ubo.terrainParams.z;
    float sinElev = ubo.sunDir.w;
    float dayFactor = ubo.sunColor.a;
    float starFactor = ubo.skyColorHorizon.a;
    float sunIntensity = ubo.atmosphereParams.z;

    vec3 skyColor = texture(uSkyViewLUT, dirToSkyViewUV(rayDir)).rgb;

    float cosElev = sign(cos((time - 6.0) / 12.0 * 3.14159)) * sqrt(max(0.0, 1.0 - sinElev * sinElev));
    vec3 trueSunDir = normalize(vec3(cosElev * 0.80, sinElev, 0.35));
    vec3 trueMoonDir = normalize(vec3(-cosElev * 0.80, -sinElev, -0.35));

    // Physical HDR Sun Disc (triggers bloom naturally above threshold)
    float sunVisibility = smoothstep(-0.04, 0.06, sinElev);
    if (sunVisibility > 0.001) {
        float sunDot = max(dot(rayDir, trueSunDir), 0.0);
        float sunDisc = smoothstep(0.99945, 0.99975, sunDot) * (14.0 * sunIntensity);
        float sunCorona = pow(sunDot, 48.0) * (1.2 * sunIntensity);
        skyColor += ubo.sunColor.rgb * (sunDisc + sunCorona) * sunVisibility;
    }

    // Moon Disc
    float moonVisibility = smoothstep(-0.04, 0.08, -sinElev);
    if (moonVisibility > 0.001) {
        float moonDot = max(dot(rayDir, trueMoonDir), 0.0);
        float moonDisc = smoothstep(0.9995, 0.9998, moonDot) * 2.5;
        float moonGlow = pow(moonDot, 36.0) * 0.35;
        skyColor += vec3(0.75, 0.86, 1.0) * (moonDisc + moonGlow) * moonVisibility;
    }

    // Twinkling stars at night
    if (starFactor > 0.01 && rayDir.y > 0.03) {
        vec3 starP = rayDir * 170.0;
        vec3 starCell = floor(starP);
        vec3 starFrac = fract(starP) - 0.5;

        float starRnd = hash31(starCell);
        float starDist = length(starFrac);

        float twinkle = sin(time * 3.0 + starRnd * 40.0) * 0.35 + 0.65;
        float starVal = smoothstep(0.11, 0.02, starDist) * step(0.965, starRnd) * twinkle * 2.5;

        float horizonFade = smoothstep(0.03, 0.25, rayDir.y);
        skyColor += vec3(starVal * horizonFade * starFactor);
    }

    // Sky-plane clouds integrated with physical sky LUT ambient
    if (rayDir.y > 0.01) {
        vec2 cloudUV = (rayDir.xz / (rayDir.y + 0.22)) * 0.22 + vec2(time * 0.006, time * 0.003);
        float c1 = fbmCloud(cloudUV);
        float c2 = fbmCloud(cloudUV * 2.4 + vec2(time * 0.003, time * 0.007));
        float cloudNoise = c1 * 0.65 + c2 * 0.35;
        float density = smoothstep(0.38, 0.72, cloudNoise);

        if (density > 0.001) {
            vec3 skyZenithSample = texture(uSkyViewLUT, dirToSkyViewUV(vec3(0.0, 0.9, 0.1))).rgb;
            vec3 skyHorizonSample = texture(uSkyViewLUT, dirToSkyViewUV(normalize(vec3(rayDir.x, 0.08, rayDir.z)))).rgb;

            float sunDot = max(dot(rayDir, ubo.sunDir.xyz), 0.0);
            float hgForward = pow(sunDot, 10.0) * 1.4 * dayFactor;
            vec3 cloudLit = skyZenithSample * 1.6 + ubo.sunColor.rgb * (0.45 + hgForward) * sunIntensity * 0.45;
            vec3 cloudShadow = skyHorizonSample * 0.55 + vec3(0.02, 0.03, 0.05) * (1.0 - dayFactor);

            vec3 cloudCol = mix(cloudShadow, cloudLit, clamp(cloudNoise * 1.15, 0.0, 1.0));
            float horizonFade = smoothstep(0.01, 0.28, rayDir.y);
            skyColor = mix(skyColor, cloudCol, density * horizonFade * 0.85);
        }
    }

    // Linear HDR output (tone mapping & gamma done in postprocess.frag)
    outColor = vec4(max(skyColor, vec3(0.0)), 1.0);
}
