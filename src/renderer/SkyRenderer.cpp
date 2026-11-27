#include "renderer/SkyRenderer.hpp"

SkyRenderer::SkyRenderer(rhi::RHIDevice& device) {
    rhi::GraphicsPipelineDesc desc{};
    desc.vertexShader = "sky.vert";
    desc.fragmentShader = "sky.frag";
    desc.cullMode = rhi::CullMode::None;
    desc.depthTestEnable = true;
    desc.depthWriteEnable = false;
    desc.depthCompareOp = rhi::CompareOp::LessOrEqual;
    desc.blendEnable = false;
    desc.colorFormat = rhi::Format::RGBA16F;
    desc.depthFormat = rhi::Format::D32F;
    desc.hasDepth = true;
    desc.usesUniformBuffer = true;
    desc.usesStorageBuffer = false;
    desc.numSampledTextures = 2; // SkyViewLUT, CloudNoise3D

    m_pipeline = device.createGraphicsPipeline(desc);
}

void SkyRenderer::recordRenderCommands(const FrameContext& frame, const AtmosphereRenderer& atmosphere) {
    rhi::RHICommandList& cmd = *frame.cmd;
    cmd.bindGraphicsPipeline(*m_pipeline);
    cmd.bindUniformBuffer(0, *frame.globalUBO);
    cmd.bindTexture(0, atmosphere.getSkyViewLUT(), atmosphere.getLinearSampler());
    cmd.bindTexture(1, atmosphere.getCloudNoise3D(), atmosphere.getCloudSampler3D());
    cmd.draw(3, 1, 0);
}
