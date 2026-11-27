#pragma once

#include "rhi/RHIDevice.hpp"
#include "renderer/FrameContext.hpp"
#include <array>
#include <memory>
#include <vector>
#include <glm/glm.hpp>

struct FoliageVertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec3 color;
};

struct FoliageInstance {
    glm::vec4 posScale;
    glm::vec4 params;
};

struct ModelMesh {
    uint32_t firstIndex = 0;
    uint32_t indexCount = 0;
};

class FoliageRenderer {
public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    explicit FoliageRenderer(rhi::RHIDevice& device);
    ~FoliageRenderer() = default;

    FoliageRenderer(const FoliageRenderer&) = delete;
    FoliageRenderer& operator=(const FoliageRenderer&) = delete;

    void updateInstances(
        rhi::RHIDevice& device,
        const std::vector<glm::vec2>& chunkOrigins,
        const TerrainConfig& config
    );

    void recordRenderCommands(const FrameContext& frame);

private:
    void createModels(rhi::RHIDevice& device);
    void createPipeline(rhi::RHIDevice& device);
    void generateScatterForChunk(
        glm::vec2 chunkOrigin,
        const TerrainConfig& config,
        std::vector<FoliageInstance>& outInstances
    );

    float sampleExactHeight(glm::vec2 worldPos, const TerrainConfig& config) const;
    glm::vec3 sampleExactNormal(glm::vec2 worldPos, const TerrainConfig& config, float h) const;

private:
    std::unique_ptr<rhi::RHIPipeline> m_pipeline;
    std::unique_ptr<rhi::RHIBuffer> m_vertexBuffer;
    std::unique_ptr<rhi::RHIBuffer> m_indexBuffer;

    std::vector<ModelMesh> m_modelMeshes;

    static constexpr size_t NUM_MODELS = 7;
    std::vector<std::vector<FoliageInstance>> m_instancesByType;
    std::array<std::array<std::unique_ptr<rhi::RHIBuffer>, MAX_FRAMES_IN_FLIGHT>, NUM_MODELS> m_instanceBuffers;
    std::array<uint32_t, NUM_MODELS> m_instanceCounts{};
};
