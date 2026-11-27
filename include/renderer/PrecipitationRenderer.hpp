#pragma once

#include "rhi/RHIDevice.hpp"
#include "renderer/FrameContext.hpp"
#include <memory>

class PrecipitationRenderer {
public:
    static constexpr uint32_t MAX_PARTICLES = 6000;

    explicit PrecipitationRenderer(rhi::RHIDevice& device);
    ~PrecipitationRenderer() = default;

    PrecipitationRenderer(const PrecipitationRenderer&) = delete;
    PrecipitationRenderer& operator=(const PrecipitationRenderer&) = delete;

    void recordRenderCommands(const FrameContext& frame);

private:
    std::unique_ptr<rhi::RHIPipeline> m_pipeline;
};

