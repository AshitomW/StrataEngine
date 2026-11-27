#include "renderer/WaterRenderer.hpp"
#include <cmath>
#include <vector>

WaterRenderer::WaterRenderer(rhi::RHIDevice& device) {
    createMesh(device);
    createPipeline(device);
}

void WaterRenderer::createMesh(rhi::RHIDevice& device) {
    const uint32_t RES = EngineConstants::Water::WATER_GRID_RES;
    const float GRID_SPAN = 2400.0f;

    std::vector<WaterVertex> vertices;
    vertices.reserve(RES * RES);

    for (uint32_t y = 0; y < RES; ++y) {
        float fY = static_cast<float>(y) / static_cast<float>(RES - 1);
        float posZ = (fY - 0.5f) * GRID_SPAN;

        for (uint32_t x = 0; x < RES; ++x) {
            float fX = static_cast<float>(x) / static_cast<float>(RES - 1);
            float posX = (fX - 0.5f) * GRID_SPAN;

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
    desc.pushConstantSize = sizeof(WaterPushConstants);
    desc.pushConstantStages = rhi::ShaderStage::Vertex | rhi::ShaderStage::Fragment;

    m_pipeline = device.createGraphicsPipeline(desc);
}

void WaterRenderer::recordRenderCommands(const FrameContext& frame) {
    const TerrainConfig& config = *frame.config;
    if (!config.showWater) return;

    rhi::RHICommandList& cmd = *frame.cmd;
    cmd.bindGraphicsPipeline(*m_pipeline);
    cmd.bindUniformBuffer(0, *frame.globalUBO);

    const uint32_t RES = EngineConstants::Water::WATER_GRID_RES;
    const float GRID_SPAN = 2400.0f;
    float cellSize = GRID_SPAN / static_cast<float>(RES - 1);
    const glm::vec3& camPos = frame.camera->getPosition();
    float snappedX = std::floor(camPos.x / cellSize) * cellSize;
    float snappedZ = std::floor(camPos.z / cellSize) * cellSize;

    WaterPushConstants pc{};
    pc.waterParams1 = glm::vec4(config.waterHeight, config.waveAmplitude, config.waveSpeed, frame.time);
    pc.waterParams2 = glm::vec4(config.waterClarity, 1.0f, config.frequency, config.debugMode);
    pc.gridCenter = glm::vec4(snappedX, snappedZ, GRID_SPAN, 0.0f);

    cmd.pushConstants(rhi::ShaderStage::Vertex | rhi::ShaderStage::Fragment, &pc, sizeof(WaterPushConstants));
    cmd.bindVertexBuffer(0, *m_vertexBuffer);
    cmd.bindIndexBuffer(*m_indexBuffer);
    cmd.drawIndexed(m_indexCount, 1, 0);
}
