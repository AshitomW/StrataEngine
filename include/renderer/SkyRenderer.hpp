#pragma once

#include "rhi/RHIDevice.hpp"
#include "renderer/AtmosphereRenderer.hpp"
#include "renderer/FrameContext.hpp"
#include <memory>

class SkyRenderer {
public:
    explicit SkyRenderer(rhi::RHIDevice& device);
    ~SkyRenderer() = default;

    SkyRenderer(const SkyRenderer&) = delete;
    SkyRenderer& operator=(const SkyRenderer&) = delete;

    void recordRenderCommands(const FrameContext& frame, const AtmosphereRenderer& atmosphere);

private:
    std::unique_ptr<rhi::RHIPipeline> m_pipeline;
};
