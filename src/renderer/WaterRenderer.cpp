#include "renderer/WaterRenderer.hpp"
#include "renderer/AtmosphereRenderer.hpp"
#include "renderer/ShadowRenderer.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

WaterRenderer::WaterRenderer(rhi::RHIDevice& device) {
    createMesh(device);
    createSimulationResources(device);
    createPipeline(device);
}

void WaterRenderer::createMesh(rhi::RHIDevice& device) {
    const uint32_t RES = 192;

    std::vector<WaterVertex> vertices;
    vertices.reserve(RES * RES);

    auto mapRadial = [](float norm) -> float {
        float a = std::abs(norm);
        float s = (norm >= 0.0f) ? 1.0f : -1.0f;
        return s * (a * 22.0f + std::pow(a, 2.5f) * 1800.0f);
    };

    for (uint32_t y = 0; y < RES; ++y) {
        float fY = static_cast<float>(y) / static_cast<float>(RES - 1);
        float posZ = mapRadial(fY * 2.0f - 1.0f);

        for (uint32_t x = 0; x < RES; ++x) {
            float fX = static_cast<float>(x) / static_cast<float>(RES - 1);
            float posX = mapRadial(fX * 2.0f - 1.0f);

            vertices.push_back({glm::vec3(posX, 0.0f, posZ), glm::vec2(fX, fY)});
        }
    }

    std::vector<uint32_t> indices;
    indices.reserve((RES - 1) * (RES - 1) * 6);

    for (uint32_t y = 0; y < RES - 1; ++y) {
        for (uint32_t x = 0; x < RES - 1; ++x) {
            uint32_t topLeft = y * RES + x;
            uint32_t topRight = topLeft + 1;
            uint32_t bottomLeft = (y + 1) * RES + x;
            uint32_t bottomRight = bottomLeft + 1;

            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);

            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }

    m_indexCount = static_cast<uint32_t>(indices.size());

    size_t vSize = sizeof(WaterVertex) * vertices.size();
    m_vertexBuffer = device.createBuffer(vSize, rhi::BufferUsage::Vertex | rhi::BufferUsage::TransferDst, rhi::MemoryUsage::GpuOnly);
    device.uploadBuffer(*m_vertexBuffer, vertices.data(), vSize);

    size_t iSize = sizeof(uint32_t) * indices.size();
    m_indexBuffer = device.createBuffer(iSize, rhi::BufferUsage::Index | rhi::BufferUsage::TransferDst, rhi::MemoryUsage::GpuOnly);
    device.uploadBuffer(*m_indexBuffer, indices.data(), iSize);
}

void WaterRenderer::createSimulationResources(rhi::RHIDevice& device) {
    rhi::SamplerDesc repeatDesc{};
    repeatDesc.minFilter = rhi::Filter::Linear;
    repeatDesc.magFilter = rhi::Filter::Linear;
    repeatDesc.addressU = rhi::AddressMode::Repeat;
    repeatDesc.addressV = rhi::AddressMode::Repeat;
    repeatDesc.addressW = rhi::AddressMode::Repeat;
    m_repeatSampler = device.createSampler(repeatDesc);

    rhi::SamplerDesc clampDesc{};
    clampDesc.minFilter = rhi::Filter::Linear;
    clampDesc.magFilter = rhi::Filter::Linear;
    clampDesc.addressU = rhi::AddressMode::ClampToEdge;
    clampDesc.addressV = rhi::AddressMode::ClampToEdge;
    clampDesc.addressW = rhi::AddressMode::ClampToEdge;
    m_clampSampler = device.createSampler(clampDesc);

    rhi::SamplerDesc nearDesc{};
    nearDesc.minFilter = rhi::Filter::Nearest;
    nearDesc.magFilter = rhi::Filter::Nearest;
    nearDesc.addressU = rhi::AddressMode::ClampToEdge;
    nearDesc.addressV = rhi::AddressMode::ClampToEdge;
    nearDesc.addressW = rhi::AddressMode::ClampToEdge;
    m_nearestSampler = device.createSampler(nearDesc);

    rhi::TextureDesc simTexDesc{};
    simTexDesc.width = SIM_RES;
    simTexDesc.height = SIM_RES;
    simTexDesc.format = rhi::Format::RGBA16F;
    simTexDesc.usage = rhi::TextureUsage::Sampled | rhi::TextureUsage::Storage;

    m_waveDispFoam = device.createTexture(simTexDesc);
    m_waveNormalSlope = device.createTexture(simTexDesc);
    m_ripples[0] = device.createTexture(simTexDesc);
    m_ripples[1] = device.createTexture(simTexDesc);

    rhi::ComputePipelineDesc specDesc{};
    specDesc.computeShader = "water_spectrum.comp";
    specDesc.numStorageBuffers = 0;
    specDesc.numSampledTextures = 0;
    specDesc.numStorageTextures = 2;
    specDesc.pushConstantSize = sizeof(WaterSimPC);
    specDesc.localSizeX = 8;
    specDesc.localSizeY = 8;
    m_spectrumPipeline = device.createComputePipeline(specDesc);

    rhi::ComputePipelineDesc ripDesc{};
    ripDesc.computeShader = "water_ripples.comp";
    ripDesc.numStorageBuffers = 0;
    ripDesc.numSampledTextures = 1;
    ripDesc.numStorageTextures = 1;
    ripDesc.pushConstantSize = sizeof(RipplePC);
    ripDesc.localSizeX = 8;
    ripDesc.localSizeY = 8;
    m_ripplesPipeline = device.createComputePipeline(ripDesc);

    device.executeImmediateCompute([&](rhi::RHICommandList& cmd) {
        cmd.textureBarrier(*m_waveDispFoam, rhi::TextureLayout::Undefined, rhi::TextureLayout::ShaderReadOnly);
        cmd.textureBarrier(*m_waveNormalSlope, rhi::TextureLayout::Undefined, rhi::TextureLayout::ShaderReadOnly);
        cmd.textureBarrier(*m_ripples[0], rhi::TextureLayout::Undefined, rhi::TextureLayout::ShaderReadOnly);
        cmd.textureBarrier(*m_ripples[1], rhi::TextureLayout::Undefined, rhi::TextureLayout::ShaderReadOnly);
    });
}

void WaterRenderer::createPipeline(rhi::RHIDevice& device) {
    rhi::GraphicsPipelineDesc desc{};
    desc.vertexShader = "water.vert";
    desc.fragmentShader = "water.frag";
    desc.vertexBindings = {
        {0, sizeof(WaterVertex), rhi::VertexInputRate::Vertex}
    };
    desc.vertexAttributes = {
        {0, 0, rhi::Format::RGB32F, static_cast<uint32_t>(offsetof(WaterVertex, pos))},
        {1, 0, rhi::Format::RG32F, static_cast<uint32_t>(offsetof(WaterVertex, uv))}
    };
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
    desc.numSampledTextures = 7; // DispFoam, Ripples, NormalSlope, ShadowAtlas, SkyViewLUT, OpaqueColor, OpaqueDepth
    desc.pushConstantSize = sizeof(WaterPushConstants);
    desc.pushConstantStages = rhi::ShaderStage::Vertex | rhi::ShaderStage::Fragment;

    m_pipeline = device.createGraphicsPipeline(desc);
}

void WaterRenderer::updateSimulation(rhi::RHICommandList& cmd, const Camera& camera, const TerrainConfig& config, float time) {
    if (!config.showWater) return;

    float dt = m_firstFrame ? 0.016f : std::clamp(time - m_lastSimTime, 0.001f, 0.033f);
    m_lastSimTime = time;

    // 1. Update JONSWAP Wave Spectrum maps
    cmd.textureBarrier(*m_waveDispFoam, rhi::TextureLayout::ShaderReadOnly, rhi::TextureLayout::General);
    cmd.textureBarrier(*m_waveNormalSlope, rhi::TextureLayout::ShaderReadOnly, rhi::TextureLayout::General);

    cmd.bindComputePipeline(*m_spectrumPipeline);
    cmd.bindStorageTexture(0, *m_waveDispFoam);
    cmd.bindStorageTexture(1, *m_waveNormalSlope);

    WaterSimPC simPC{};
    simPC.simParams = glm::vec4(time, config.waveAmplitude, config.waveSpeed, config.windSpeed);
    simPC.windDirPatch = glm::vec4(0.80f, 0.60f, PATCH_SIZE, config.waveChoppiness);
    cmd.pushConstants(rhi::ShaderStage::Compute, &simPC, sizeof(WaterSimPC));

    cmd.dispatch(SIM_RES / 8, SIM_RES / 8, 1);

    cmd.textureBarrier(*m_waveDispFoam, rhi::TextureLayout::General, rhi::TextureLayout::ShaderReadOnly);
    cmd.textureBarrier(*m_waveNormalSlope, rhi::TextureLayout::General, rhi::TextureLayout::ShaderReadOnly);

    // 2. Update 2D Interactive Ripple wave equation
    const glm::vec3& camPos = camera.getPosition();
    glm::vec2 currCenter(std::floor(camPos.x * 2.0f) * 0.5f, std::floor(camPos.z * 2.0f) * 0.5f);
    glm::vec2 uvDelta = m_firstFrame ? glm::vec2(0.0f) : (currCenter - m_lastRippleCenter) / RIPPLE_SPAN;
    m_lastRippleCenter = currCenter;
    m_firstFrame = false;

    uint32_t prevIdx = m_rippleIndex;
    uint32_t nextIdx = 1u - m_rippleIndex;
    m_rippleIndex = nextIdx;

    cmd.textureBarrier(*m_ripples[nextIdx], rhi::TextureLayout::ShaderReadOnly, rhi::TextureLayout::General);
    cmd.bindComputePipeline(*m_ripplesPipeline);
    cmd.bindTexture(0, *m_ripples[prevIdx], *m_clampSampler);
    cmd.bindStorageTexture(1, *m_ripples[nextIdx]);

    float distToWater = std::abs(camPos.y - config.waterHeight);
    float camWake = (config.interactiveRipples && distToWater < 4.0f) ? (1.0f - distToWater / 4.0f) : 0.0f;

    RipplePC ripPC{};
    ripPC.rippleParams = glm::vec4(time, dt, config.interactiveRipples ? (0.20f + config.precipitationIntensity * 1.6f) : 0.0f, 2.2f);
    ripPC.cameraDelta = glm::vec4(uvDelta.x, uvDelta.y, 0.0f, camWake);
    cmd.pushConstants(rhi::ShaderStage::Compute, &ripPC, sizeof(RipplePC));

    cmd.dispatch(SIM_RES / 8, SIM_RES / 8, 1);
    cmd.textureBarrier(*m_ripples[nextIdx], rhi::TextureLayout::General, rhi::TextureLayout::ShaderReadOnly);
}

void WaterRenderer::recordRenderCommands(const FrameContext& frame) {
    const TerrainConfig& config = *frame.config;
    if (!config.showWater || !frame.opaqueColor || !frame.opaqueDepth || !frame.atmosphere || !frame.shadows) return;

    rhi::RHICommandList& cmd = *frame.cmd;
    cmd.bindGraphicsPipeline(*m_pipeline);
    cmd.bindUniformBuffer(0, *frame.globalUBO);

    cmd.bindTexture(0, *m_waveDispFoam, *m_repeatSampler);
    cmd.bindTexture(1, *m_ripples[m_rippleIndex], *m_clampSampler);
    cmd.bindTexture(2, *m_waveNormalSlope, *m_repeatSampler);
    cmd.bindTexture(3, frame.shadows->getShadowAtlas(), *m_nearestSampler);
    cmd.bindTexture(4, frame.atmosphere->getSkyViewLUT(), frame.atmosphere->getLinearSampler());
    cmd.bindTexture(5, *frame.opaqueColor, *m_clampSampler);
    cmd.bindTexture(6, *frame.opaqueDepth, *m_nearestSampler);

    const glm::vec3& camPos = frame.camera->getPosition();
    float snappedX = std::floor(camPos.x * 2.0f) * 0.5f;
    float snappedZ = std::floor(camPos.z * 2.0f) * 0.5f;

    WaterPushConstants pc{};
    pc.waterParams1 = glm::vec4(config.waterHeight, config.waveAmplitude, config.waveSpeed, frame.time);
    pc.waterParams2 = glm::vec4(config.waterClarity, config.windSpeed, config.waveChoppiness, config.debugMode);
    pc.gridCenter = glm::vec4(snappedX, snappedZ, m_lastRippleCenter.x, m_lastRippleCenter.y);

    cmd.pushConstants(rhi::ShaderStage::Vertex | rhi::ShaderStage::Fragment, &pc, sizeof(WaterPushConstants));
    cmd.bindVertexBuffer(0, *m_vertexBuffer);
    cmd.bindIndexBuffer(*m_indexBuffer);
    cmd.drawIndexed(m_indexCount, 1, 0);
}
