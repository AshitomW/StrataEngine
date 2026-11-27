#pragma once

#include "rhi/RHIDevice.hpp"
#include "camera/Camera.hpp"
#include "terrain/ChunkManager.hpp"
#include "renderer/FoliageRenderer.hpp"
#include "terrain/TerrainTypes.hpp"
#include <array>
#include <memory>

class ShadowRenderer {
public:
    static constexpr uint32_t CASCADE_COUNT = 4;
    static constexpr uint32_t TILE_RES = 2048;
    static constexpr uint32_t ATLAS_RES = TILE_RES * 2; // 4096x4096

    explicit ShadowRenderer(rhi::RHIDevice& device);
    ~ShadowRenderer() = default;

    ShadowRenderer(const ShadowRenderer&) = delete;
    ShadowRenderer& operator=(const ShadowRenderer&) = delete;

    void updateCascades(const Camera& camera, const glm::vec3& lightDir);

    void renderShadows(
        rhi::RHICommandList& cmd,
        const rhi::RHIBuffer& globalUBO,
        ChunkManager& chunkManager,
        FoliageRenderer& foliageRenderer,
        uint32_t frameIndex,
        const TerrainConfig& config
    );

    const std::array<glm::mat4, CASCADE_COUNT>& getLightSpaceMatrices() const { return m_lightSpaceMatrices; }
    const glm::vec4& getCascadeSplits() const { return m_cascadeSplits; }
    const rhi::RHITexture& getShadowAtlas() const { return *m_shadowAtlas; }
    const rhi::RHISampler& getShadowSampler() const { return *m_shadowSampler; }

private:
    std::unique_ptr<rhi::RHITexture> m_shadowAtlas;
    std::unique_ptr<rhi::RHISampler> m_shadowSampler;
    std::unique_ptr<rhi::RHIPipeline> m_terrainShadowPipeline;
    std::unique_ptr<rhi::RHIPipeline> m_foliageShadowPipeline;

    std::array<glm::mat4, CASCADE_COUNT> m_lightSpaceMatrices{};
    glm::vec4 m_cascadeSplits{32.0f, 105.0f, 280.0f, 720.0f};
};
