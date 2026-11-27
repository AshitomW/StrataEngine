#pragma once

#include "rhi/RHIDevice.hpp"
#include "renderer/FrameContext.hpp"
#include <array>
#include <memory>
#include <vector>
#include <glm/glm.hpp>

struct StructureVertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec3 color;
    glm::vec2 matParams; // x: roughness, y: emissive
};

struct StructureInstance {
    glm::vec4 posScale;
    glm::vec4 params; // x: yaw, y: type, z: tint, w: unused
};

struct StructureMeshRange {
    uint32_t firstIndex = 0;
    uint32_t indexCount = 0;
};

class StructureRenderer {
public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
    static constexpr size_t NUM_STRUCTURES = 5;

    explicit StructureRenderer(rhi::RHIDevice& device);
    ~StructureRenderer() = default;

    StructureRenderer(const StructureRenderer&) = delete;
    StructureRenderer& operator=(const StructureRenderer&) = delete;

    void updateInstances(
        rhi::RHIDevice& device,
        const std::vector<glm::vec2>& chunkOrigins,
        const TerrainConfig& config
    );

    void recordRenderCommands(const FrameContext& frame);
    void recordShadowCommands(rhi::RHICommandList& cmd, const rhi::RHIBuffer& globalUBO, uint32_t frameIndex, uint32_t cascadeIndex);

private:
    void createMeshes(rhi::RHIDevice& device);
    void createPipelines(rhi::RHIDevice& device);
    void generateForChunk(
        glm::vec2 chunkOrigin,
        const TerrainConfig& config,
        std::vector<StructureInstance>& outInstances
    );

private:
    std::unique_ptr<rhi::RHIPipeline> m_pipeline;
    std::unique_ptr<rhi::RHIPipeline> m_shadowPipeline;
    std::unique_ptr<rhi::RHIBuffer> m_vertexBuffer;
    std::unique_ptr<rhi::RHIBuffer> m_indexBuffer;

    std::array<StructureMeshRange, NUM_STRUCTURES> m_meshes{};
    std::array<std::vector<StructureInstance>, NUM_STRUCTURES> m_instancesByType;
    std::array<std::array<std::unique_ptr<rhi::RHIBuffer>, MAX_FRAMES_IN_FLIGHT>, NUM_STRUCTURES> m_instanceBuffers;
    std::array<uint32_t, NUM_STRUCTURES> m_instanceCounts{};
};

