#pragma once

#include "terrain/TerrainTypes.hpp"
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>

namespace TerrainNoiseCPU {

inline uint32_t hashCPU(glm::uvec2 p, uint32_t seed) {
    uint32_t h = seed;
    h ^= p.x + 0x9e3779b9u + (h << 6) + (h >> 2);
    h ^= p.y + 0x9e3779b9u + (h << 6) + (h >> 2);
    h ^= h >> 16;
    h *= 0x85ebca6bu;
    h ^= h >> 13;
    h *= 0xc2b2ae35u;
    h ^= h >> 16;
    return h;
}

inline glm::vec2 hash2CPU(glm::vec2 p, uint32_t seed) {
    glm::uvec2 ip = glm::uvec2(glm::ivec2(glm::floor(p)) + 100000);
    uint32_t h1 = hashCPU(ip, seed);
    uint32_t h2 = hashCPU(ip + glm::uvec2(137u, 269u), seed);
    return glm::vec2(
        static_cast<float>(h1 & 0x00FFFFFFu) / 16777215.0f,
        static_cast<float>(h2 & 0x00FFFFFFu) / 16777215.0f
    ) * 2.0f - 1.0f;
}

inline float gradientNoiseCPU(glm::vec2 p, uint32_t seed) {
    glm::vec2 i = glm::floor(p);
    glm::vec2 f = glm::fract(p);
    glm::vec2 u = f * f * f * (f * (f * 6.0f - 15.0f) + 10.0f);

    float n00 = glm::dot(hash2CPU(i + glm::vec2(0.0f, 0.0f), seed), f - glm::vec2(0.0f, 0.0f));
    float n10 = glm::dot(hash2CPU(i + glm::vec2(1.0f, 0.0f), seed), f - glm::vec2(1.0f, 0.0f));
    float n01 = glm::dot(hash2CPU(i + glm::vec2(0.0f, 1.0f), seed), f - glm::vec2(0.0f, 1.0f));
    float n11 = glm::dot(hash2CPU(i + glm::vec2(1.0f, 1.0f), seed), f - glm::vec2(1.0f, 1.0f));

    return glm::mix(glm::mix(n00, n10, u.x), glm::mix(n01, n11, u.x), u.y);
}

inline float fbmCPU(glm::vec2 p, uint32_t seed, int octaves, float lacunarity, float persistence) {
    float sum = 0.0f;
    float amp = 1.0f;
    float maxAmp = 0.0f;
    float freq = 1.0f;

    for (int i = 0; i < octaves; ++i) {
        sum += amp * gradientNoiseCPU(p * freq, seed + static_cast<uint32_t>(i * 31));
        maxAmp += amp;
        freq *= lacunarity;
        amp *= persistence;
    }

    return sum / maxAmp;
}

inline float evaluatePresetHeight(glm::vec2 worldPos, uint32_t presetType, float baseFreq, float amp, float warpStrength, float mountainPower, uint32_t seed) {
    if (presetType == 0u) {
        float mFreq = baseFreq * 2.2f;
        glm::vec2 p = worldPos * mFreq;

        glm::vec2 q = glm::vec2(
            fbmCPU(p * 0.5f, seed + 11u, 4, 2.0f, 0.5f),
            fbmCPU(p * 0.5f + glm::vec2(5.2f, 1.3f), seed + 22u, 4, 2.0f, 0.5f)
        );
        glm::vec2 warped = p + warpStrength * q;

        float chain = fbmCPU(warped * 0.35f, seed + 33u, 4, 2.0f, 0.5f);
        float chainMask = glm::smoothstep(-0.30f, 0.35f, chain);

        float r1 = 1.0f - std::abs(gradientNoiseCPU(warped * 0.75f, seed + 101u));
        float r2 = 1.0f - std::abs(gradientNoiseCPU(warped * 1.60f, seed + 102u));
        float r3 = 1.0f - std::abs(gradientNoiseCPU(warped * 3.30f, seed + 103u));
        float r4 = 1.0f - std::abs(gradientNoiseCPU(warped * 6.80f, seed + 104u));

        float peakStructure = std::pow(std::max(0.0f, r1 * 0.50f + r2 * 0.30f + r3 * 0.14f + r4 * 0.06f), 2.1f) * 2.2f;
        float valley = std::abs(gradientNoiseCPU(warped * 0.45f + glm::vec2(2.3f, 7.1f), seed + 200u));
        float valleyCarve = glm::smoothstep(0.04f, 0.60f, valley);
        float lakeBasin = fbmCPU(warped * 0.25f, seed + 250u, 3, 2.0f, 0.5f);
        float lakeDip = glm::smoothstep(-0.4f, 0.2f, lakeBasin);

        float h = glm::mix(chain * 0.15f - 0.05f, peakStructure * mountainPower * valleyCarve, chainMask * lakeDip);
        return (h * 0.88f + 0.04f) * amp;
    } else if (presetType == 1u) {
        float hFreq = baseFreq * 4.2f;
        glm::vec2 p = worldPos * hFreq;

        glm::vec2 q = glm::vec2(
            gradientNoiseCPU(p * 0.35f, seed + 10u),
            gradientNoiseCPU(p * 0.35f + glm::vec2(4.1f, 2.7f), seed + 20u)
        );
        glm::vec2 warped = p + warpStrength * 0.35f * q;

        float hill1 = std::sin(warped.x * 0.65f) * std::cos(warped.y * 0.65f) * 0.25f;
        float hill2 = (gradientNoiseCPU(warped * 0.75f, seed + 30u) * 0.5f + 0.5f);
        float hill3 = (gradientNoiseCPU(warped * 1.50f, seed + 40u) * 0.5f + 0.5f) * 0.35f;
        float hill4 = (gradientNoiseCPU(warped * 3.00f, seed + 50u) * 0.5f + 0.5f) * 0.12f;
        float hill5 = gradientNoiseCPU(warped * 6.00f, seed + 60u) * 0.04f;

        float river = std::abs(gradientNoiseCPU(warped * 0.25f + glm::vec2(1.2f, 8.4f), seed + 70u));
        float riverCarve = glm::smoothstep(0.02f, 0.25f, river);
        float rollingMounds = std::pow(std::max(0.0f, hill2 + hill3 + hill4 + hill1), 1.30f) * 0.50f + hill5;
        return ((rollingMounds * riverCarve) * 0.85f + 0.08f) * amp;
    } else if (presetType == 2u) {
        float cFreq = baseFreq * 2.8f;
        glm::vec2 p = worldPos * cFreq;

        glm::vec2 q = glm::vec2(
            fbmCPU(p * 0.55f, seed + 15u, 3, 2.0f, 0.5f),
            fbmCPU(p * 0.55f + glm::vec2(7.3f, 1.9f), seed + 25u, 3, 2.0f, 0.5f)
        );
        glm::vec2 warped = p + warpStrength * 0.9f * q;

        float baseMesa = fbmCPU(warped * 0.5f, seed + 50u, 4, 2.0f, 0.5f);
        float numSteps = 8.0f;
        float stepped = std::floor(baseMesa * numSteps) / numSteps;
        float frac = (baseMesa * numSteps) - std::floor(baseMesa * numSteps);
        float terracedMesa = stepped + glm::smoothstep(0.0f, 0.28f, frac) / numSteps;

        float chasm1 = std::abs(fbmCPU(warped * 0.70f, seed + 70u, 4, 2.0f, 0.5f));
        float chasm2 = std::abs(fbmCPU(warped * 1.35f + glm::vec2(3.1f, 5.7f), seed + 80u, 3, 2.0f, 0.5f));
        float gorge = std::min(chasm1, chasm2);
        float gorgeCarve = glm::smoothstep(0.04f, 0.35f, gorge);

        float buttes = 1.0f - std::abs(gradientNoiseCPU(warped * 1.7f, seed + 90u));
        buttes = std::pow(std::max(0.0f, buttes), 3.2f) * 0.30f;
        return ((terracedMesa * gorgeCarve + buttes) * 0.85f + 0.06f) * amp;
    } else if (presetType == 3u) {
        float iFreq = baseFreq * 2.6f;
        glm::vec2 p = worldPos * iFreq;

        glm::vec2 q = glm::vec2(
            gradientNoiseCPU(p * 0.5f, seed + 12u),
            gradientNoiseCPU(p * 0.5f + glm::vec2(3.7f, 8.2f), seed + 24u)
        );
        glm::vec2 warped = p + warpStrength * 0.55f * q;

        float oceanBase = fbmCPU(warped * 0.45f, seed + 10u, 4, 2.0f, 0.5f);
        float islandPeak = std::pow(std::max(0.0f, 1.0f - std::abs(gradientNoiseCPU(warped * 1.25f, seed + 60u))), 2.0f);
        float volcanoCrater = std::abs(gradientNoiseCPU(warped * 2.5f, seed + 70u)) * 0.25f;

        float islandMask = glm::smoothstep(-0.06f, 0.35f, oceanBase);
        return glm::mix(-0.25f, (islandPeak * 1.3f - volcanoCrater) * 1.25f, islandMask) * amp;
    } else if (presetType == 5u) {
        float aFreq = baseFreq * 2.2f;
        glm::vec2 p = worldPos * aFreq;
        glm::vec2 q = glm::vec2(
            fbmCPU(p * 0.45f, seed + 19u, 3, 2.0f, 0.5f),
            fbmCPU(p * 0.45f + glm::vec2(4.7f, 9.1f), seed + 29u, 3, 2.0f, 0.5f)
        );
        glm::vec2 warped = p + warpStrength * 0.75f * q;
        float uValley = std::abs(fbmCPU(warped * 0.42f, seed + 301u, 3, 2.0f, 0.5f));
        float valleyProfile = glm::smoothstep(0.06f, 0.40f, uValley);
        float spires = std::pow(std::max(0.0f, 1.0f - std::abs(gradientNoiseCPU(warped * 1.15f, seed + 302u))), 2.35f) * 1.45f;
        float rimNoise = (1.0f - std::abs(gradientNoiseCPU(warped * 2.3f, seed + 303u))) * 0.35f;
        float h = glm::mix(-0.04f, (spires * mountainPower * 0.78f + rimNoise), valleyProfile) + 0.02f;
        return h * amp;
    } else if (presetType == 6u) {
        float tFreq = baseFreq * 2.5f;
        glm::vec2 p = worldPos * tFreq;
        glm::vec2 q = glm::vec2(
            fbmCPU(p * 0.5f, seed + 41u, 3, 2.0f, 0.5f),
            fbmCPU(p * 0.5f + glm::vec2(8.1f, 2.3f), seed + 43u, 3, 2.0f, 0.5f)
        );
        glm::vec2 warped = p + warpStrength * 0.65f * q;
        float plateau = fbmCPU(warped * 0.60f, seed + 401u, 4, 2.0f, 0.5f) * 0.5f + 0.5f;
        float fjord = std::abs(gradientNoiseCPU(warped * 0.45f, seed + 402u));
        float fjordCut = glm::smoothstep(0.05f, 0.36f, fjord);
        float frostRidge = std::pow(std::max(0.0f, 1.0f - std::abs(gradientNoiseCPU(warped * 1.4f, seed + 403u))), 1.9f) * 0.55f;
        float h = glm::mix(-0.22f, plateau * 0.58f + frostRidge * mountainPower * 0.45f, fjordCut) + 0.04f;
        return h * amp;
    } else if (presetType == 7u) {
        float ahFreq = baseFreq * 3.0f;
        glm::vec2 p = worldPos * ahFreq;
        glm::vec2 q = glm::vec2(gradientNoiseCPU(p * 0.4f, seed + 501u), gradientNoiseCPU(p * 0.4f + glm::vec2(3.1f, 7.7f), seed + 502u));
        glm::vec2 warped = p + warpStrength * 0.55f * q;
        float rolling = fbmCPU(warped * 0.55f, seed + 503u, 4, 2.0f, 0.5f) * 0.5f + 0.5f;
        float crag = std::pow(std::max(0.0f, 1.0f - std::abs(gradientNoiseCPU(warped * 1.35f, seed + 504u))), 1.8f) * 0.55f;
        float loch = glm::smoothstep(0.04f, 0.28f, std::abs(gradientNoiseCPU(warped * 0.32f, seed + 505u)));
        float h = (rolling * 0.65f + crag * 0.45f * mountainPower * 0.5f) * loch - 0.03f;
        return h * amp;
    } else {
        float kFreq = baseFreq * 2.9f;
        glm::vec2 p = worldPos * kFreq;
        glm::vec2 q = glm::vec2(gradientNoiseCPU(p * 0.45f, seed + 601u), gradientNoiseCPU(p * 0.45f + glm::vec2(5.4f, 2.8f), seed + 602u));
        glm::vec2 warped = p + warpStrength * 0.55f * q;
        float towerCell = 1.0f - std::abs(gradientNoiseCPU(warped * 1.25f, seed + 603u));
        float tower = std::pow(glm::smoothstep(0.34f, 0.92f, towerCell), 1.55f) * 1.15f;
        float microCliff = gradientNoiseCPU(warped * 3.2f, seed + 604u) * 0.06f;
        float river = glm::smoothstep(0.03f, 0.22f, std::abs(gradientNoiseCPU(warped * 0.28f, seed + 605u)));
        float h = (tower * mountainPower * 0.62f + microCliff * tower) * river + 0.04f;
        return h * amp;
    }
}

inline float sampleHeight(glm::vec2 worldPos, const TerrainConfig& config) {
    float baseFreq = config.frequency * 0.004f;
    float amp = config.amplitude;
    float warpStrength = config.warpStrength;
    float mountainPower = config.mountainPower;
    uint32_t seed = config.seed;
    uint32_t presetType = config.presetType;

    if (presetType != 4u) {
        return evaluatePresetHeight(worldPos, presetType, baseFreq, amp, warpStrength, mountainPower, seed);
    }

    glm::vec2 macroP = worldPos * 0.00045f;
    float continent = fbmCPU(macroP, seed + 500u, 4, 2.0f, 0.5f);
    float moisture  = fbmCPU(macroP + glm::vec2(12.3f, 45.6f), seed + 600u, 4, 2.0f, 0.5f);
    float temp      = fbmCPU(macroP + glm::vec2(78.9f, 13.4f), seed + 700u, 3, 2.0f, 0.5f);

    float wOcean = glm::smoothstep(0.0f, -0.25f, continent);
    float wHighland = glm::smoothstep(-0.05f, 0.25f, continent);
    float wMid = 1.0f - std::max(wOcean, wHighland);

    float coldMask = glm::smoothstep(0.05f, -0.22f, temp);
    float humidMask = glm::smoothstep(-0.05f, 0.22f, moisture);
    float wTundra  = wHighland * coldMask;
    float wCanyon  = wHighland * (1.0f - coldMask) * (1.0f - humidMask) * 0.55f;
    float wMountain = wHighland * (1.0f - coldMask) * (1.0f - humidMask) * 0.45f;
    float wKarst   = wHighland * (1.0f - coldMask) * humidMask;
    float wAutumn  = wMid * glm::smoothstep(-0.05f, 0.20f, temp);
    float wHills   = wMid * (1.0f - glm::smoothstep(-0.05f, 0.20f, temp));

    float hMtn = evaluatePresetHeight(worldPos, 0u, baseFreq, amp * 1.10f, warpStrength, mountainPower, seed);
    float hHill = evaluatePresetHeight(worldPos, 1u, baseFreq, amp * 0.40f, warpStrength * 0.5f, mountainPower * 0.6f, seed);
    float hCanyon = evaluatePresetHeight(worldPos, 2u, baseFreq, amp * 0.85f, warpStrength, mountainPower, seed);
    float hIsland = evaluatePresetHeight(worldPos, 3u, baseFreq, amp * 0.65f, warpStrength, mountainPower, seed);
    float hTundra = evaluatePresetHeight(worldPos, 6u, baseFreq, amp * 0.72f, warpStrength, mountainPower, seed);
    float hAutumn = evaluatePresetHeight(worldPos, 7u, baseFreq, amp * 0.65f, warpStrength, mountainPower, seed);
    float hKarst  = evaluatePresetHeight(worldPos, 8u, baseFreq, amp * 0.95f, warpStrength, mountainPower, seed);

    float totalWeight = wOcean + wHills + wAutumn + wMountain + wCanyon + wTundra + wKarst + 0.0001f;
    return (hIsland * wOcean + hHill * wHills + hAutumn * wAutumn + hMtn * wMountain + hCanyon * wCanyon + hTundra * wTundra + hKarst * wKarst) / totalWeight;
}

inline glm::vec3 sampleNormal(glm::vec2 worldPos, const TerrainConfig& config) {
    float eps = CHUNK_CELL_SIZE * 0.5f;
    float hL = sampleHeight(worldPos - glm::vec2(eps, 0.0f), config);
    float hR = sampleHeight(worldPos + glm::vec2(eps, 0.0f), config);
    float hD = sampleHeight(worldPos - glm::vec2(0.0f, eps), config);
    float hU = sampleHeight(worldPos + glm::vec2(0.0f, eps), config);

    return glm::normalize(glm::vec3((hL - hR) / (2.0f * eps), 1.0f, (hD - hU) / (2.0f * eps)));
}

inline uint32_t sampleEffectiveBiome(glm::vec2 worldXZ, const TerrainConfig& config) {
    if (config.presetType != 4u) return config.presetType;
    glm::vec2 macroP = worldXZ * 0.00045f;
    float continent = fbmCPU(macroP, config.seed + 500u, 4, 2.0f, 0.5f);
    float moisture  = fbmCPU(macroP + glm::vec2(12.3f, 45.6f), config.seed + 600u, 4, 2.0f, 0.5f);
    float temp      = fbmCPU(macroP + glm::vec2(78.9f, 13.4f), config.seed + 700u, 3, 2.0f, 0.5f);

    if (continent < -0.12f) return 3u;
    if (continent > 0.10f) {
        if (temp < -0.10f) return 6u;
        if (moisture > 0.08f) return 8u;
        if (moisture < -0.05f) return 2u;
        return 0u;
    }
    return (temp > 0.08f) ? 7u : 1u;
}

} // namespace TerrainNoiseCPU

