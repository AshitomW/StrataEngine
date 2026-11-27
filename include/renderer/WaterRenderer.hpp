#pragma once

#include "rhi/RHIDevice.hpp"
#include "renderer/FrameContext.hpp"
#include <memory>

struct WaterVertex {
    glm::vec3 pos;
    glm::vec2 uv;
};

class WaterRenderer {
public:
    explicit WaterRenderer(rhi::RHIDevice& device);
    ~WaterRenderer() = default;

    WaterRenderer(const WaterRenderer&) = delete;
    WaterRenderer& operator=(const WaterRenderer&) = delete;

    void recordRenderCommands(const FrameContext& frame);

private:
    void createMesh(rhi::RHIDevice& device);
    void createPipeline(rhi::RHIDevice& device);

private:
    std::unique_ptr<rhi::RHIPipeline> m_pipeline;
    std::unique_ptr<rhi::RHIBuffer> m_vertexBuffer;
    std::unique_ptr<rhi::RHIBuffer> m_indexBuffer;
    uint32_t m_indexCount = 0;
};
