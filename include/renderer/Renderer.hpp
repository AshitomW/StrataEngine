#pragma once

#include "rhi/RHIDevice.hpp"
#include "core/Frustum.hpp"
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
    void updateUBO(uint32_t frameIndex, const Camera& camera, const TerrainConfig& config, float time);
    void drawImGui(const Camera& camera, ChunkManager& chunkManager, TerrainConfig& config, HUDInfo& hudInfo);

private:
    rhi::RHIDevice& m_device;

    std::unique_ptr<FoliageRenderer> m_foliageRenderer;
    std::unique_ptr<SkyRenderer> m_skyRenderer;
    std::unique_ptr<WaterRenderer> m_waterRenderer;

    std::array<std::unique_ptr<rhi::RHIBuffer>, MAX_FRAMES_IN_FLIGHT> m_uboBuffers;
    std::unique_ptr<rhi::RHIPipeline> m_solidPipeline;
    std::unique_ptr<rhi::RHIPipeline> m_wireframePipeline;

    FoliageKey m_lastFoliageKey;
};
