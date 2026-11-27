#include "renderer/MaterialLibrary.hpp"

MaterialLibrary::MaterialLibrary(rhi::RHIDevice& device) {
    rhi::TextureDesc desc{};
    desc.width = TEXTURE_RES;
    desc.height = TEXTURE_RES;
    desc.arrayLayers = LAYER_COUNT;
    desc.format = rhi::Format::RGBA8_UNORM;
    desc.type = rhi::TextureType::Tex2DArray;
    desc.usage = rhi::TextureUsage::Sampled | rhi::TextureUsage::Storage;

    m_albedoHeightArray = device.createTexture(desc);
    m_normalORMArray = device.createTexture(desc);

    rhi::SamplerDesc sampDesc{};
    sampDesc.minFilter = rhi::Filter::Linear;
    sampDesc.magFilter = rhi::Filter::Linear;
    sampDesc.addressU = rhi::AddressMode::Repeat;
    sampDesc.addressV = rhi::AddressMode::Repeat;
    sampDesc.addressW = rhi::AddressMode::ClampToEdge;
    sampDesc.maxAnisotropy = 16.0f;
    m_materialSampler = device.createSampler(sampDesc);

    rhi::ComputePipelineDesc pipeDesc{};
    pipeDesc.computeShader = "material_bake.comp";
    pipeDesc.numStorageBuffers = 0;
    pipeDesc.numSampledTextures = 0;
    pipeDesc.numStorageTextures = 2;
    pipeDesc.localSizeX = 8;
    pipeDesc.localSizeY = 8;
    pipeDesc.localSizeZ = 1;
    m_bakePipeline = device.createComputePipeline(pipeDesc);

    device.executeImmediateCompute([&](rhi::RHICommandList& cmd) {
        cmd.textureBarrier(*m_albedoHeightArray, rhi::TextureLayout::Undefined, rhi::TextureLayout::General);
        cmd.textureBarrier(*m_normalORMArray, rhi::TextureLayout::Undefined, rhi::TextureLayout::General);

        cmd.bindComputePipeline(*m_bakePipeline);
        cmd.bindStorageTexture(0, *m_albedoHeightArray);
        cmd.bindStorageTexture(1, *m_normalORMArray);
        cmd.dispatch(TEXTURE_RES / 8, TEXTURE_RES / 8, LAYER_COUNT);

        cmd.textureBarrier(*m_albedoHeightArray, rhi::TextureLayout::General, rhi::TextureLayout::ShaderReadOnly);
        cmd.textureBarrier(*m_normalORMArray, rhi::TextureLayout::General, rhi::TextureLayout::ShaderReadOnly);
    });
}

