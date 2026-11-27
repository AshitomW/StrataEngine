#include "renderer/AtmosphereRenderer.hpp"

AtmosphereRenderer::AtmosphereRenderer(rhi::RHIDevice& device) {
    rhi::SamplerDesc sampDesc{};
    sampDesc.minFilter = rhi::Filter::Linear;
    sampDesc.magFilter = rhi::Filter::Linear;
    sampDesc.addressU = rhi::AddressMode::ClampToEdge;
    sampDesc.addressV = rhi::AddressMode::ClampToEdge;
    sampDesc.addressW = rhi::AddressMode::ClampToEdge;
    m_linearSampler = device.createSampler(sampDesc);

    rhi::SamplerDesc cloudSamp{};
    cloudSamp.minFilter = rhi::Filter::Linear;
    cloudSamp.magFilter = rhi::Filter::Linear;
    cloudSamp.addressU = rhi::AddressMode::Repeat;
    cloudSamp.addressV = rhi::AddressMode::Repeat;
    cloudSamp.addressW = rhi::AddressMode::Repeat;
    m_cloudSampler3D = device.createSampler(cloudSamp);

    rhi::TextureDesc transDesc{};
    transDesc.width = 256;
    transDesc.height = 64;
    transDesc.format = rhi::Format::RGBA16F;
    transDesc.usage = rhi::TextureUsage::Sampled | rhi::TextureUsage::Storage;
    m_transmittanceLUT = device.createTexture(transDesc);

    rhi::TextureDesc msDesc{};
    msDesc.width = 32;
    msDesc.height = 32;
    msDesc.format = rhi::Format::RGBA16F;
    msDesc.usage = rhi::TextureUsage::Sampled | rhi::TextureUsage::Storage;
    m_multiScatterLUT = device.createTexture(msDesc);

    rhi::TextureDesc svDesc{};
    svDesc.width = 192;
    svDesc.height = 108;
    svDesc.format = rhi::Format::RGBA16F;
    svDesc.usage = rhi::TextureUsage::Sampled | rhi::TextureUsage::Storage;
    m_skyViewLUT = device.createTexture(svDesc);

    rhi::TextureDesc cloudDesc{};
    cloudDesc.width = 64;
    cloudDesc.height = 64;
    cloudDesc.depth = 64;
    cloudDesc.format = rhi::Format::RGBA8_UNORM;
    cloudDesc.type = rhi::TextureType::Tex3D;
    cloudDesc.usage = rhi::TextureUsage::Sampled | rhi::TextureUsage::Storage;
    m_cloudNoise3D = device.createTexture(cloudDesc);

    rhi::ComputePipelineDesc tPipe{};
    tPipe.computeShader = "atmo_transmittance.comp";
    tPipe.numStorageBuffers = 0;
    tPipe.numSampledTextures = 0;
    tPipe.numStorageTextures = 1;
    tPipe.localSizeX = 8;
    tPipe.localSizeY = 8;
    m_transmittancePipeline = device.createComputePipeline(tPipe);

    rhi::ComputePipelineDesc msPipe{};
    msPipe.computeShader = "atmo_multiscatter.comp";
    msPipe.numStorageBuffers = 0;
    msPipe.numSampledTextures = 1;
    msPipe.numStorageTextures = 1;
    msPipe.localSizeX = 8;
    msPipe.localSizeY = 8;
    m_multiScatterPipeline = device.createComputePipeline(msPipe);

    rhi::ComputePipelineDesc svPipe{};
    svPipe.computeShader = "atmo_skyview.comp";
    svPipe.numStorageBuffers = 0;
    svPipe.numSampledTextures = 2;
    svPipe.numStorageTextures = 1;
    svPipe.pushConstantSize = sizeof(AtmoPushConstants);
    svPipe.localSizeX = 8;
    svPipe.localSizeY = 8;
    m_skyViewPipeline = device.createComputePipeline(svPipe);

    rhi::ComputePipelineDesc cbPipe{};
    cbPipe.computeShader = "cloud_noise_bake.comp";
    cbPipe.numStorageBuffers = 0;
    cbPipe.numSampledTextures = 0;
    cbPipe.numStorageTextures = 1;
    cbPipe.localSizeX = 4;
    cbPipe.localSizeY = 4;
    cbPipe.localSizeZ = 4;
    m_cloudBakePipeline = device.createComputePipeline(cbPipe);

    device.executeImmediateCompute([&](rhi::RHICommandList& cmd) {
        cmd.textureBarrier(*m_transmittanceLUT, rhi::TextureLayout::Undefined, rhi::TextureLayout::General);
        cmd.bindComputePipeline(*m_transmittancePipeline);
        cmd.bindStorageTexture(0, *m_transmittanceLUT);
        cmd.dispatch(256 / 8, 64 / 8, 1);
        cmd.textureBarrier(*m_transmittanceLUT, rhi::TextureLayout::General, rhi::TextureLayout::ShaderReadOnly);

        cmd.textureBarrier(*m_multiScatterLUT, rhi::TextureLayout::Undefined, rhi::TextureLayout::General);
        cmd.bindComputePipeline(*m_multiScatterPipeline);
        cmd.bindTexture(0, *m_transmittanceLUT, *m_linearSampler);
        cmd.bindStorageTexture(1, *m_multiScatterLUT);
        cmd.dispatch(32 / 8, 32 / 8, 1);
        cmd.textureBarrier(*m_multiScatterLUT, rhi::TextureLayout::General, rhi::TextureLayout::ShaderReadOnly);

        cmd.textureBarrier(*m_cloudNoise3D, rhi::TextureLayout::Undefined, rhi::TextureLayout::General);
        cmd.bindComputePipeline(*m_cloudBakePipeline);
        cmd.bindStorageTexture(0, *m_cloudNoise3D);
        cmd.dispatch(64 / 4, 64 / 4, 64 / 4);
        cmd.textureBarrier(*m_cloudNoise3D, rhi::TextureLayout::General, rhi::TextureLayout::ShaderReadOnly);

        cmd.textureBarrier(*m_skyViewLUT, rhi::TextureLayout::Undefined, rhi::TextureLayout::ShaderReadOnly);
    });
}

void AtmosphereRenderer::updateSkyView(
    rhi::RHICommandList& cmd,
    const glm::vec3& trueSunDir,
    const glm::vec3& sunColor,
    float turbidity,
    float mieAnisotropy,
    float sunIntensity
) {
    cmd.textureBarrier(*m_skyViewLUT, rhi::TextureLayout::ShaderReadOnly, rhi::TextureLayout::General);

    cmd.bindComputePipeline(*m_skyViewPipeline);
    cmd.bindTexture(0, *m_transmittanceLUT, *m_linearSampler);
    cmd.bindTexture(1, *m_multiScatterLUT, *m_linearSampler);
    cmd.bindStorageTexture(2, *m_skyViewLUT);

    AtmoPushConstants pc{};
    pc.sunDirTurbidity = glm::vec4(trueSunDir, turbidity);
    pc.sunColorAnisotropy = glm::vec4(sunColor * sunIntensity, mieAnisotropy);
    cmd.pushConstants(rhi::ShaderStage::Compute, &pc, sizeof(AtmoPushConstants));

    cmd.dispatch((192 + 7) / 8, (108 + 7) / 8, 1);

    cmd.textureBarrier(*m_skyViewLUT, rhi::TextureLayout::General, rhi::TextureLayout::ShaderReadOnly);
}
