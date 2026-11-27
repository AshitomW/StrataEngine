#pragma once

#include "rhi/RHIDevice.hpp"
#include "core/Frustum.hpp"
#include "renderer/AtmosphereRenderer.hpp"
#include "renderer/FoliageRenderer.hpp"
#include "renderer/SkyRenderer.hpp"
#include "renderer/WaterRenderer.hpp"
#include "renderer/FrameContext.hpp"
#include "renderer/Atmosphere.hpp"
#include "camera/Camera.hpp"
#include "terrain/ChunkManager.hpp"
#include "terrain/TerrainTypes.hpp"
#include <array>
#include <memory>
#include <string>

struct HUDInfo {
    float fps = 0.0f;
    size_t activeChunks = 0;
    bool showHUD = true;
    bool mouseCaptured = true;
    std::string presetName = "Mountains";
};

struct FoliageKey {
    uint32_t seed = 0;
    int centerChunkX = -9999;
    int centerChunkZ = -9999;
    int radius = -1;
    bool showFoliage = true;
    uint32_t presetType = 9999;
    float waterHeight = -9999.0f;
    float foliageDensity = -1.0f;

    bool operator==(const FoliageKey& o) const {
        return seed == o.seed &&
            centerChunkX == o.centerChunkX &&
            centerChunkZ == o.centerChunkZ &&
            radius == o.radius &&
            showFoliage == o.showFoliage &&
            presetType == o.presetType &&
            waterHeight == o.waterHeight &&
            foliageDensity == o.foliageDensity;
    }
};

class Renderer {
public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
    static constexpr uint32_t BLOOM_MIP_COUNT = 5;

    Renderer(rhi::RHIDevice& device, GLFWwindow* window);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool renderFrame(
        GLFWwindow* window,
        const Camera& camera,
        ChunkManager& chunkManager,
        TerrainConfig& config,
        HUDInfo& hudInfo,
        float time
    );

    float getAspectRatio() const;

private:
    void createUBOBuffers();
    void createPipelines();
    void createScreenResources(uint32_t width, uint32_t height);
    void updateUBO(uint32_t frameIndex, const Camera& camera, const TerrainConfig& config, float time);
    void runBloomPasses(rhi::RHICommandList& cmd, const TerrainConfig& config);
    void drawImGui(const Camera& camera, ChunkManager& chunkManager, TerrainConfig& config, HUDInfo& hudInfo);

private:
    rhi::RHIDevice& m_device;

    std::unique_ptr<AtmosphereRenderer> m_atmosphereRenderer;
    std::unique_ptr<FoliageRenderer> m_foliageRenderer;
    std::unique_ptr<SkyRenderer> m_skyRenderer;
    std::unique_ptr<WaterRenderer> m_waterRenderer;

    std::array<std::unique_ptr<rhi::RHIBuffer>, MAX_FRAMES_IN_FLIGHT> m_uboBuffers;
    std::unique_ptr<rhi::RHIPipeline> m_solidPipeline;
    std::unique_ptr<rhi::RHIPipeline> m_wireframePipeline;

    // HDR offscreen & post-process resources
    uint32_t m_screenWidth = 0;
    uint32_t m_screenHeight = 0;
    std::unique_ptr<rhi::RHITexture> m_hdrColor;
    std::unique_ptr<rhi::RHITexture> m_sceneDepth;
    std::unique_ptr<rhi::RHITexture> m_blackFallbackTex;
    std::array<std::unique_ptr<rhi::RHITexture>, BLOOM_MIP_COUNT> m_bloomDownMips;
    std::array<std::unique_ptr<rhi::RHITexture>, BLOOM_MIP_COUNT - 1> m_bloomUpMips;
    std::unique_ptr<rhi::RHISampler> m_linearSampler;

    std::unique_ptr<rhi::RHIPipeline> m_bloomDownPipeline;
    std::unique_ptr<rhi::RHIPipeline> m_bloomUpPipeline;
    std::unique_ptr<rhi::RHIPipeline> m_postProcessPipeline;

    FoliageKey m_lastFoliageKey;
};
