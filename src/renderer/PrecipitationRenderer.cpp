#include "renderer/PrecipitationRenderer.hpp"

PrecipitationRenderer::PrecipitationRenderer(rhi::RHIDevice& device) {
    rhi::GraphicsPipelineDesc desc{};
    desc.vertexShader = "precipitation.vert";
    desc.fragmentShader = "precipitation.frag";
    desc.cullMode = rhi::CullMode::None;
    desc.depthTestEnable = true;
    desc.depthWriteEnable = false;
    desc.depthCompareOp = rhi::CompareOp::LessOrEqual;
    desc.blendEnable = true;
    desc.colorFormat = rhi::Format::RGBA16F;
    desc.depthFormat = rhi::Format::D32F;
    desc.hasDepth = true;
    desc.usesUniformBuffer = true;
    desc.usesStorageBuffer = false;

    m_pipeline = device.createGraphicsPipeline(desc);
}

void PrecipitationRenderer::recordRenderCommands(const FrameContext& frame) {
    const TerrainConfig& config = *frame.config;
    if (config.precipitationIntensity <= 0.01f || config.precipitationType < 0.5f) {
        return;
    }

    rhi::RHICommandList& cmd = *frame.cmd;
    cmd.bindGraphicsPipeline(*m_pipeline);
    cmd.bindUniformBuffer(0, *frame.globalUBO);
    cmd.draw(6, MAX_PARTICLES, 0);
}

