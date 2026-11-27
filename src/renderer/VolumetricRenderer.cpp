#include "renderer/VolumetricRenderer.hpp"
#include <algorithm>

VolumetricRenderer::VolumetricRenderer(rhi::RHIDevice& device, uint32_t width, uint32_t height)
    : m_device(device),
      m_width(std::max(1u, width / 4u)),
      m_height(std::max(1u, height / 4u)) {
    rhi::ComputePipelineDesc cDesc{};
    cDesc.computeShader = "volumetric_rays.comp";
    cDesc.localSizeX = 8;
    cDesc.localSizeY = 8;
    cDesc.localSizeZ = 1;
    cDesc.numStorageBuffers = 1;
    cDesc.numSampledTextures = 2;
    cDesc.numStorageTextures = 1;
    m_pipeline = m_device.createComputePipeline(cDesc);

    m_paramsBuffer = m_device.createBuffer(
        sizeof(GodRaysData),
        rhi::BufferUsage::Storage | rhi::BufferUsage::TransferDst,
        rhi::MemoryUsage::CpuToGpu
    );

    rhi::SamplerDesc pDesc{};
    pDesc.minFilter = rhi::Filter::Nearest;
    pDesc.magFilter = rhi::Filter::Nearest;
    pDesc.addressU = rhi::AddressMode::ClampToEdge;
    pDesc.addressV = rhi::AddressMode::ClampToEdge;
    pDesc.addressW = rhi::AddressMode::ClampToEdge;
    m_pointSampler = m_device.createSampler(pDesc);

    rhi::SamplerDesc lDesc{};
    lDesc.minFilter = rhi::Filter::Linear;
    lDesc.magFilter = rhi::Filter::Linear;
    lDesc.addressU = rhi::AddressMode::ClampToEdge;
    lDesc.addressV = rhi::AddressMode::ClampToEdge;
    lDesc.addressW = rhi::AddressMode::ClampToEdge;
    m_linearSampler = m_device.createSampler(lDesc);

    createImages();
}

void VolumetricRenderer::createImages() {
    rhi::TextureDesc td{};
    td.width = m_width;
    td.height = m_height;
    td.format = rhi::Format::RGBA16F;
    td.usage = rhi::TextureUsage::Sampled | rhi::TextureUsage::Storage;
    m_godRaysTex = m_device.createTexture(td);
}

void VolumetricRenderer::resize(uint32_t width, uint32_t height) {
    uint32_t qw = std::max(1u, width / 4u);
    uint32_t qh = std::max(1u, height / 4u);
    if (qw == m_width && qh == m_height && m_godRaysTex) {
        return;
    }
    m_width = qw;
    m_height = qh;
    createImages();
}

void VolumetricRenderer::compute(
    rhi::RHICommandList& cmd,
    const rhi::RHITexture& depthTex,
    const rhi::RHITexture& shadowAtlas,
    const GlobalUBO& ubo,
    const TerrainConfig& config,
    uint32_t frameIndex
) {
    GodRaysData data{};
    data.invViewProj = glm::inverse(ubo.viewProj);
    for (int i = 0; i < 4; ++i) {
        data.lightSpaceMatrices[i] = ubo.lightSpaceMatrices[i];
    }
    data.cameraPos = glm::vec4(glm::vec3(ubo.cameraPos), ubo.cascadeSplits.x);
    data.sunDir = glm::vec4(glm::vec3(ubo.sunDir), ubo.cascadeSplits.y);
    data.sunColor = glm::vec4(glm::vec3(ubo.sunColor), ubo.atmosphereParams.z);
    data.params0 = glm::vec4(
        ubo.cascadeSplits.z,
        ubo.cascadeSplits.w,
        static_cast<float>(frameIndex % 64u),
        config.godRaysEnabled ? 1.0f : 0.0f
    );
    data.params1 = glm::vec4(
        220.0f,
        0.0085f * config.fogMultiplier,
        0.65f,
        config.waterHeight
    );
    m_paramsBuffer->copyFromHost(&data, sizeof(GodRaysData));

    cmd.textureBarrier(*m_godRaysTex, rhi::TextureLayout::Undefined, rhi::TextureLayout::General);

    cmd.bindComputePipeline(*m_pipeline);
    cmd.bindStorageBuffer(0, *m_paramsBuffer);
    cmd.bindTexture(1, depthTex, *m_pointSampler);
    cmd.bindTexture(2, shadowAtlas, *m_pointSampler);
    cmd.bindStorageTexture(3, *m_godRaysTex);
    cmd.dispatch((m_width + 7) / 8, (m_height + 7) / 8, 1);

    cmd.textureBarrier(*m_godRaysTex, rhi::TextureLayout::General, rhi::TextureLayout::ShaderReadOnly);
}
