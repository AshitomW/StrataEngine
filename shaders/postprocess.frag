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
    vec4 postParams;       // x: exposure, y: bloomIntensity, z: bloomThreshold, w: toneMapper
    vec4 atmosphereParams;
} ubo;

layout(set = 2, binding = 0) uniform sampler2D uHdrColor;
layout(set = 2, binding = 1) uniform sampler2D uBloomTex;

vec3 agxDefaultContrastApprox(vec3 x) {
    vec3 x2 = x * x;
    vec3 x4 = x2 * x2;
    return + 15.5     * x4 * x2
           - 40.14    * x4 * x
           + 31.96    * x4
           - 6.868    * x2 * x
           + 0.4298   * x2
           + 0.1191   * x
           - 0.00232;
}

vec3 agx_tonemap(vec3 val) {
    const mat3 agxMat = mat3(
        0.842479062253094, 0.0423282422610123, 0.0423756549057051,
        0.0784335999999992, 0.878468636469772, 0.0784336,
        0.0792237451477643, 0.0791661274605434, 0.879142973793104
    );
    const mat3 agxMatInv = mat3(
        1.19687900512017, -0.0528968517574562, -0.0529716355144438,
        -0.0980208811401368, 1.15190312990417, -0.0980434501171241,
        -0.0990297440797205, -0.0989611768448433, 1.15107367264116
    );
    const float minEv = -12.47393;
    const float maxEv = 4.026069;

    val = agxMat * max(val, vec3(1e-6));
    val = clamp(log2(val), minEv, maxEv);
    val = (val - minEv) / (maxEv - minEv);
    val = agxDefaultContrastApprox(val);
    val = agxMatInv * val;
    val = clamp(val, 0.0, 1.0);
    // AgX output is sRGB-encoded; convert back to linear for hardware sRGB swapchain
    return pow(val, vec3(2.2));
}

vec3 aces_film(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec3 reinhard_extended(vec3 x, float whitePt) {
    vec3 num = x * (1.0 + x / (whitePt * whitePt));
    return clamp(num / (1.0 + x), 0.0, 1.0);
}

void main() {
    vec3 hdr = texture(uHdrColor, inUV).rgb;
    vec3 bloom = texture(uBloomTex, inUV).rgb;

    float exposure = max(ubo.postParams.x, 0.05);
    float bloomIntensity = max(ubo.postParams.y, 0.0);
    int toneMapper = int(ubo.postParams.w + 0.5);

    vec3 color = (hdr + bloom * bloomIntensity) * exposure;

    // Subtle edge vignette
    vec2 centered = inUV * 2.0 - 1.0;
    float vignette = 1.0 - dot(centered, centered) * 0.08;
    color *= clamp(vignette, 0.75, 1.0);

    vec3 mapped;
    if (toneMapper == 0) {
        mapped = agx_tonemap(color);
    } else if (toneMapper == 1) {
        mapped = aces_film(color);
    } else {
        mapped = reinhard_extended(color, 4.0);
    }

    // Swapchain is BGRA8_SRGB so linear output is hardware sRGB encoded
    outColor = vec4(mapped, 1.0);
}
