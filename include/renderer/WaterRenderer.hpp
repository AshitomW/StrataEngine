#pragma once

#include "rhi/RHIDevice.hpp"
#include "camera/Camera.hpp"
#include "renderer/FrameContext.hpp"
#include <array>
#include <memory>

struct WaterVertex {
    glm::vec3 pos;
    glm::vec2 uv;
};

class WaterRenderer {
public:
    static constexpr uint32_t SIM_RES = 256;
    static constexpr float PATCH_SIZE = 64.0f;
    static constexpr float RIPPLE_SPAN = 48.0f;

    explicit WaterRenderer(rhi::RHIDevice& device);
    ~WaterRenderer() = default;

    WaterRenderer(const WaterRenderer&) = delete;
    WaterRenderer& operator=(const WaterRenderer&) = delete;

    void updateSimulation(rhi::RHICommandList& cmd, const Camera& camera, const TerrainConfig& config, float time);
    void recordRenderCommands(const FrameContext& frame);

private:
    void createMesh(rhi::RHIDevice& device);
    void createSimulationResources(rhi::RHIDevice& device);
    void createPipeline(rhi::RHIDevice& device);

private:
    struct WaterSimPC {
        glm::vec4 simParams;
        glm::vec4 windDirPatch;
    };

    struct RipplePC {
        glm::vec4 rippleParams;
        glm::vec4 cameraDelta;
    };

    std::unique_ptr<rhi::RHIPipeline> m_pipeline;
    std::unique_ptr<rhi::RHIPipeline> m_spectrumPipeline;
    std::unique_ptr<rhi::RHIPipeline> m_ripplesPipeline;

    std::unique_ptr<rhi::RHIBuffer> m_vertexBuffer;
    std::unique_ptr<rhi::RHIBuffer> m_indexBuffer;
    uint32_t m_indexCount = 0;

    std::unique_ptr<rhi::RHITexture> m_waveDispFoam;
    std::unique_ptr<rhi::RHITexture> m_waveNormalSlope;
    std::array<std::unique_ptr<rhi::RHITexture>, 2> m_ripples;
    uint32_t m_rippleIndex = 0;

    std::unique_ptr<rhi::RHISampler> m_repeatSampler;
    std::unique_ptr<rhi::RHISampler> m_clampSampler;
    std::unique_ptr<rhi::RHISampler> m_nearestSampler;

    glm::vec2 m_lastRippleCenter{0.0f, 0.0f};
    float m_lastSimTime = 0.0f;
    bool m_firstFrame = true;
};
