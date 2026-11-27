#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include "core/EngineConstants.hpp"

static constexpr uint32_t CHUNK_GRID_RES = EngineConstants::Terrain::GRID_RESOLUTION;
static constexpr float CHUNK_SIZE = EngineConstants::Terrain::CHUNK_SIZE;
static constexpr float CHUNK_CELL_SIZE = EngineConstants::Terrain::CELL_SIZE;
static constexpr uint32_t NUM_LOD_LEVELS = EngineConstants::Terrain::NUM_LOD_LEVELS;

struct TerrainConfig {
    uint32_t seed = EngineConstants::Terrain::DEFAULT_SEED;
    float frequency = 1.0f;
    float amplitude = EngineConstants::Presets::MOUNTAINS.amplitude;
    float warpStrength = EngineConstants::Presets::MOUNTAINS.warpStrength;
    float mountainPower = EngineConstants::Presets::MOUNTAINS.mountainPower;
    float octaves = 6.0f;
    float lacunarity = 2.0f;
    float persistence = 0.45f;
    float waterHeight = EngineConstants::Presets::MOUNTAINS.waterHeight;
    float fogDensity = 0.00065f;
    float debugMode = 0.0f;
    uint32_t presetType = EngineConstants::Presets::MOUNTAINS.type;
    int lodMode = -1;
    int viewRadius = EngineConstants::Terrain::DEFAULT_VIEW_RADIUS;
    bool wireframe = false;
    bool showFoliage = true;
    float foliageDensity = EngineConstants::Environment::DEFAULT_FOLIAGE_DENSITY;

    float timeOfDay = EngineConstants::Environment::DEFAULT_TIME_OF_DAY;
    bool timeCycleRunning = true;
    float timeCycleSpeed = EngineConstants::Environment::DEFAULT_CYCLE_SPEED;

    bool isDynamicLOD() const { return lodMode == -1; }

    float getViewDistance() const {
        return static_cast<float>(2 * viewRadius + 1) * CHUNK_SIZE * 0.5f;
    }

    void increaseViewDistance() {
        viewRadius = std::min(EngineConstants::Terrain::MAX_VIEW_RADIUS, viewRadius + 1);
        updateFog();
    }

    void decreaseViewDistance() {
        viewRadius = std::max(EngineConstants::Terrain::MIN_VIEW_RADIUS, viewRadius - 1);
        updateFog();
    }

    void increaseWaterHeight() {
        waterHeight += 1.0f;
    }

    void decreaseWaterHeight() {
        waterHeight -= 1.0f;
    }

    void toggleFoliage() {
        showFoliage = !showFoliage;
    }

    void increaseFoliageDensity() {
        foliageDensity = std::min(EngineConstants::Environment::MAX_FOLIAGE_DENSITY, foliageDensity + 0.25f);
    }

    void decreaseFoliageDensity() {
        foliageDensity = std::max(0.0f, foliageDensity - 0.25f);
    }

    void advanceTime(float deltaSeconds) {
        if (timeCycleRunning) {
            timeOfDay = std::fmod(timeOfDay + deltaSeconds * timeCycleSpeed, 24.0f);
            if (timeOfDay < 0.0f) timeOfDay += 24.0f;
        }
    }

    void scrubTime(float deltaHours) {
        timeOfDay = std::fmod(timeOfDay + deltaHours, 24.0f);
        if (timeOfDay < 0.0f) timeOfDay += 24.0f;
    }

    void toggleTimeCycle() {
        timeCycleRunning = !timeCycleRunning;
    }

    void updateFog() {
        float visibleDist = static_cast<float>(viewRadius) * CHUNK_SIZE * 0.95f;
        fogDensity = 1.0f / std::max(50.0f, visibleDist);
    }

    void cycleLODMode() {
        lodMode++;
        if (lodMode >= static_cast<int>(NUM_LOD_LEVELS)) {
            lodMode = -1;
        }
    }

    void decreaseLOD() {
        if (lodMode == -1) {
            lodMode = 0;
        } else {
            lodMode = std::max(0, lodMode - 1);
        }
    }

    void increaseLOD() {
        if (lodMode == -1) {
            lodMode = 1;
        } else {
            lodMode = std::min(static_cast<int>(NUM_LOD_LEVELS) - 1, lodMode + 1);
        }
    }

    void applyPreset(const EngineConstants::Presets::BiomePresetDef& def) {
        presetType = def.type;
        amplitude = def.amplitude;
        frequency = def.frequency;
        warpStrength = def.warpStrength;
        mountainPower = def.mountainPower;
        waterHeight = def.waterHeight;
        updateFog();
    }

    void applyMountains() {
        applyPreset(EngineConstants::Presets::MOUNTAINS);
    }

    void applyHills() {
        applyPreset(EngineConstants::Presets::HILLS);
    }

    void applyCanyons() {
        applyPreset(EngineConstants::Presets::CANYONS);
    }

    void applyIslands() {
        applyPreset(EngineConstants::Presets::ISLANDS);
    }

    void applyMultiBiome() {
        applyPreset(EngineConstants::Presets::MULTIBIOME);
    }

    void applyAlpineValley() {
        applyPreset(EngineConstants::Presets::ALPINE_VALLEY);
    }

    void applyTundra() {
        applyPreset(EngineConstants::Presets::TUNDRA);
    }

    void applyAutumnHighlands() {
        applyPreset(EngineConstants::Presets::AUTUMN_HIGHLANDS);
    }

    void applyKarstPeaks() {
        applyPreset(EngineConstants::Presets::KARST_PEAKS);
    }

    bool showWater = true;
    float waveAmplitude = EngineConstants::Water::DEFAULT_WAVE_AMPLITUDE;
    float waveSpeed = EngineConstants::Water::DEFAULT_WAVE_SPEED;
    float waterClarity = EngineConstants::Water::DEFAULT_WATER_CLARITY;
    float windSpeed = 9.0f;
    float waveChoppiness = 0.85f;
    bool interactiveRipples = true;

    // Milestone 2: HDR, Physical Atmosphere & Post-Processing
    float exposure = 1.0f;
    bool bloomEnabled = true;
    float bloomIntensity = 0.32f;
    float bloomThreshold = 1.15f;
    int toneMapper = 0; // 0: AgX, 1: ACES, 2: Reinhard
    float turbidity = 1.0f;
    float mieAnisotropy = 0.78f;
    float sunIntensity = 2.6f;
    float iblStrength = 1.0f;

    // Milestone 3: Shadows, GTAO & Baked PBR Textures
    bool shadowsEnabled = true;
    float shadowStrength = 1.0f;
    float shadowBias = 0.0012f;
    bool gtaoEnabled = true;
    float gtaoStrength = 0.75f;
    bool usePBRTextures = true;

    // Milestone 5: Dynamic Weather, Volumetric Clouds & Procedural Structures
    int weatherType = 1; // 0: Clear, 1: PartlyCloudy, 2: Overcast, 3: Rain, 4: Thunderstorm, 5: Snow, 6: Foggy
    bool weatherAutoTransition = true;
    float cloudCoverage = 0.42f;
    float wetness = 0.0f;
    float snowAccumulation = 0.0f;
    float precipitationIntensity = 0.0f;
    float precipitationType = 0.0f; // 0: none, 1: rain, 2: snow
    float fogMultiplier = 1.0f;
    float lightningFlash = 0.0f;

    bool showStructures = false;
    float structureDensity = 0.0f;

    bool ssrEnabled = true;
    bool godRaysEnabled = true;
    float godRaysIntensity = 0.55f;
    bool cdlodMorph = true;
    bool sharpenEnabled = true;
    int chunkBudgetPerFrame = 16;

    void toggleWater() {
        showWater = !showWater;
    }

    void increaseWaveAmplitude() {
        waveAmplitude = std::min(2.5f, waveAmplitude + 0.15f);
    }

    void decreaseWaveAmplitude() {
        waveAmplitude = std::max(0.0f, waveAmplitude - 0.15f);
    }

    void increaseWaveSpeed() {
        waveSpeed = std::min(3.0f, waveSpeed + 0.25f);
    }

    void decreaseWaveSpeed() {
        waveSpeed = std::max(0.0f, waveSpeed - 0.25f);
    }
};

struct GlobalUBO {
    glm::mat4 viewProj;
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec4 cameraPos;
    glm::vec4 sunDir;
    glm::vec4 sunColor;
    glm::vec4 skyColorZenith;
    glm::vec4 skyColorHorizon;
    glm::vec4 terrainParams;
    glm::vec4 biomeParams;
    glm::vec4 postParams;
    glm::vec4 atmosphereParams;
    glm::mat4 lightSpaceMatrices[4];
    glm::vec4 cascadeSplits;
    glm::vec4 shadowParams;
    glm::vec4 weatherParams;
    glm::vec4 weatherParams2;
    glm::vec4 tracingParams; // x: ssrEnabled, y: godRaysIntensity, z: cdlodMorph, w: sharpenEnabled
};

struct ShadowPushConstants {
    glm::uvec4 cascadeInfo;
};

struct ChunkPushConstants {
    glm::vec4 chunkOffset;
    glm::uvec4 lodParams;
};

struct ComputePushConstants {
    glm::vec4 chunkOffset;
    glm::vec4 noiseParams1;
    glm::vec4 noiseParams2;
    glm::uvec4 config;
};

struct WaterPushConstants {
    glm::vec4 waterParams1;
    glm::vec4 waterParams2;
    glm::vec4 gridCenter;
};
