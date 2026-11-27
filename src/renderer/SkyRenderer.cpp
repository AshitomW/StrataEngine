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
    desc.usesUniformBuffer = true;
    desc.usesStorageBuffer = false;

    m_pipeline = device.createGraphicsPipeline(desc);
}

void SkyRenderer::recordRenderCommands(const FrameContext& frame) {
    rhi::RHICommandList& cmd = *frame.cmd;
    cmd.bindGraphicsPipeline(*m_pipeline);
    cmd.bindUniformBuffer(0, *frame.globalUBO);
    cmd.draw(3, 1, 0);
}
