#include "renderer/GTAORenderer.hpp"
#include <algorithm>

GTAORenderer::GTAORenderer(rhi::RHIDevice& device) {
    rhi::SamplerDesc sampDesc{};
    sampDesc.minFilter = rhi::Filter::Linear;
    sampDesc.magFilter = rhi::Filter::Linear;
    sampDesc.addressU = rhi::AddressMode::ClampToEdge;
    sampDesc.addressV = rhi::AddressMode::ClampToEdge;
    sampDesc.addressW = rhi::AddressMode::ClampToEdge;
    m_sampler = device.createSampler(sampDesc);

    rhi::ComputePipelineDesc gtaoDesc{};
    gtaoDesc.computeShader = "gtao.comp";
    gtaoDesc.numStorageBuffers = 0;
    gtaoDesc.numSampledTextures = 1;
    gtaoDesc.numStorageTextures = 1;
    gtaoDesc.pushConstantSize = sizeof(GTAOPushConstants);
    gtaoDesc.localSizeX = 8;
    gtaoDesc.localSizeY = 8;
    m_gtaoPipeline = device.createComputePipeline(gtaoDesc);

    rhi::ComputePipelineDesc blurDesc{};
    blurDesc.computeShader = "gtao_blur.comp";
    blurDesc.numStorageBuffers = 0;
    blurDesc.numSampledTextures = 2;
    blurDesc.numStorageTextures = 1;
    blurDesc.localSizeX = 8;
    blurDesc.localSizeY = 8;
    m_blurPipeline = device.createComputePipeline(blurDesc);

    resize(device, device.getSwapchainWidth(), device.getSwapchainHeight());
}

void GTAORenderer::resize(rhi::RHIDevice& device, uint32_t fullWidth, uint32_t fullHeight) {
    uint32_t hw = std::max(1u, fullWidth / 2);
    uint32_t hh = std::max(1u, fullHeight / 2);
    if (hw == m_halfWidth && hh == m_halfHeight) return;

    m_halfWidth = hw;
    m_halfHeight = hh;

    rhi::TextureDesc desc{};
    desc.width = hw;
    desc.height = hh;
    desc.format = rhi::Format::R16F;
    desc.usage = rhi::TextureUsage::Sampled | rhi::TextureUsage::Storage;

    m_aoRaw = device.createTexture(desc);
    m_aoBlur = device.createTexture(desc);

    device.executeImmediateCompute([&](rhi::RHICommandList& cmd) {
        cmd.textureBarrier(*m_aoRaw, rhi::TextureLayout::Undefined, rhi::TextureLayout::ShaderReadOnly);
        cmd.textureBarrier(*m_aoBlur, rhi::TextureLayout::Undefined, rhi::TextureLayout::ShaderReadOnly);
    });
}

void GTAORenderer::compute(
    rhi::RHICommandList& cmd,
    const rhi::RHITexture& sceneDepth,
    const Camera& camera,
    uint32_t frameIndex,
    const TerrainConfig& config
) {
    if (!config.gtaoEnabled || config.gtaoStrength <= 0.001f) return;

    cmd.textureBarrier(*m_aoRaw, rhi::TextureLayout::ShaderReadOnly, rhi::TextureLayout::General);
    cmd.bindComputePipeline(*m_gtaoPipeline);
    cmd.bindTexture(0, sceneDepth, *m_sampler);
    cmd.bindStorageTexture(1, *m_aoRaw);

    GTAOPushConstants pc{};
    pc.invProj = glm::inverse(camera.getProjMatrix());
    pc.params = glm::vec4(static_cast<float>(m_halfWidth), static_cast<float>(m_halfHeight), 3.0f, static_cast<float>(frameIndex));
    cmd.pushConstants(rhi::ShaderStage::Compute, &pc, sizeof(GTAOPushConstants));

    cmd.dispatch((m_halfWidth + 7) / 8, (m_halfHeight + 7) / 8, 1);
    cmd.textureBarrier(*m_aoRaw, rhi::TextureLayout::General, rhi::TextureLayout::ShaderReadOnly);

    cmd.textureBarrier(*m_aoBlur, rhi::TextureLayout::ShaderReadOnly, rhi::TextureLayout::General);
    cmd.bindComputePipeline(*m_blurPipeline);
    cmd.bindTexture(0, *m_aoRaw, *m_sampler);
    cmd.bindTexture(1, sceneDepth, *m_sampler);
    cmd.bindStorageTexture(2, *m_aoBlur);
    cmd.dispatch((m_halfWidth + 7) / 8, (m_halfHeight + 7) / 8, 1);
    cmd.textureBarrier(*m_aoBlur, rhi::TextureLayout::General, rhi::TextureLayout::ShaderReadOnly);
}
