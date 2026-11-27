#include "renderer/StructureRenderer.hpp"
#include "renderer/ShadowRenderer.hpp"
#include "terrain/TerrainNoiseCPU.hpp"
#include <cmath>
#include <random>

StructureRenderer::StructureRenderer(rhi::RHIDevice& device) {
    createMeshes(device);
    createPipelines(device);
}

void StructureRenderer::createMeshes(rhi::RHIDevice& device) {
    std::vector<StructureVertex> vertices;
    std::vector<uint32_t> indices;

    auto addBox = [&](glm::vec3 minP, glm::vec3 maxP, glm::vec3 col, float rough, float emissive) {
        struct Face { glm::vec3 n; glm::vec3 v[4]; };
        Face faces[6] = {
            {{ 0,  0,  1}, {{minP.x, minP.y, maxP.z}, {maxP.x, minP.y, maxP.z}, {maxP.x, maxP.y, maxP.z}, {minP.x, maxP.y, maxP.z}}},
            {{ 0,  0, -1}, {{maxP.x, minP.y, minP.z}, {minP.x, minP.y, minP.z}, {minP.x, maxP.y, minP.z}, {maxP.x, maxP.y, minP.z}}},
            {{ 1,  0,  0}, {{maxP.x, minP.y, maxP.z}, {maxP.x, minP.y, minP.z}, {maxP.x, maxP.y, minP.z}, {maxP.x, maxP.y, maxP.z}}},
            {{-1,  0,  0}, {{minP.x, minP.y, minP.z}, {minP.x, minP.y, maxP.z}, {minP.x, maxP.y, maxP.z}, {minP.x, maxP.y, minP.z}}},
            {{ 0,  1,  0}, {{minP.x, maxP.y, maxP.z}, {maxP.x, maxP.y, maxP.z}, {maxP.x, maxP.y, minP.z}, {minP.x, maxP.y, minP.z}}},
            {{ 0, -1,  0}, {{minP.x, minP.y, minP.z}, {maxP.x, minP.y, minP.z}, {maxP.x, minP.y, maxP.z}, {minP.x, minP.y, maxP.z}}}
        };
        for (const auto& f : faces) {
            uint32_t base = static_cast<uint32_t>(vertices.size());
            for (int i = 0; i < 4; ++i) {
                vertices.push_back({f.v[i], f.n, col, glm::vec2(rough, emissive)});
            }
            indices.push_back(base + 0); indices.push_back(base + 1); indices.push_back(base + 2);
            indices.push_back(base + 0); indices.push_back(base + 2); indices.push_back(base + 3);
        }
    };

    auto addTaperedCyl = [&](float rBot, float rTop, float minY, float maxY, int sides, glm::vec3 col, float rough, float emissive) {
        uint32_t base = static_cast<uint32_t>(vertices.size());
        for (int i = 0; i < sides; ++i) {
            float a = static_cast<float>(i) / static_cast<float>(sides) * 6.2831853f;
            float ca = std::cos(a), sa = std::sin(a);
            glm::vec3 n = glm::normalize(glm::vec3(ca, (rBot - rTop) / std::max(maxY - minY, 0.1f), sa));
            vertices.push_back({glm::vec3(ca * rBot, minY, sa * rBot), n, col, glm::vec2(rough, emissive)});
            vertices.push_back({glm::vec3(ca * rTop, maxY, sa * rTop), n, col, glm::vec2(rough, emissive)});
        }
        for (int i = 0; i < sides; ++i) {
            int next = (i + 1) % sides;
            indices.push_back(base + i * 2); indices.push_back(base + i * 2 + 1); indices.push_back(base + next * 2);
            indices.push_back(base + next * 2); indices.push_back(base + i * 2 + 1); indices.push_back(base + next * 2 + 1);
        }
    };

    auto addConeRoof = [&](float baseY, float topY, float radius, int sides, glm::vec3 col, float rough) {
        uint32_t tip = static_cast<uint32_t>(vertices.size());
        vertices.push_back({glm::vec3(0.0f, topY, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), col, glm::vec2(rough, 0.0f)});
        uint32_t base = static_cast<uint32_t>(vertices.size());
        for (int i = 0; i < sides; ++i) {
            float a = static_cast<float>(i) / static_cast<float>(sides) * 6.2831853f;
            glm::vec3 n = glm::normalize(glm::vec3(std::cos(a), 0.65f, std::sin(a)));
            vertices.push_back({glm::vec3(std::cos(a) * radius, baseY, std::sin(a) * radius), n, col, glm::vec2(rough, 0.0f)});
        }
        for (int i = 0; i < sides; ++i) {
            int next = (i + 1) % sides;
            indices.push_back(tip); indices.push_back(base + i); indices.push_back(base + next);
        }
    };

    // 0: Alpine Stone Watchtower
    m_meshes[0].firstIndex = static_cast<uint32_t>(indices.size());
    glm::vec3 darkStone(0.20f, 0.21f, 0.24f);
    glm::vec3 towerStone(0.32f, 0.33f, 0.36f);
    glm::vec3 slateRoof(0.14f, 0.18f, 0.24f);
    glm::vec3 warmGlow(1.0f, 0.68f, 0.25f);
    addBox({-2.6f, -3.5f, -2.6f}, {2.6f, 0.5f, 2.6f}, darkStone, 0.90f, 0.0f);
    addTaperedCyl(2.2f, 1.75f, 0.5f, 9.5f, 8, towerStone, 0.88f, 0.0f);
    addTaperedCyl(2.1f, 2.15f, 9.5f, 10.6f, 8, darkStone, 0.85f, 0.0f);
    for (int b = 0; b < 4; ++b) {
        float ang = b * 1.570796f;
        float cx = std::cos(ang) * 1.85f, cz = std::sin(ang) * 1.85f;
        addBox({cx - 0.35f, 10.6f, cz - 0.35f}, {cx + 0.35f, 11.4f, cz + 0.35f}, towerStone, 0.88f, 0.0f);
    }
    addBox({-0.55f, 10.6f, -0.55f}, {0.55f, 11.8f, 0.55f}, warmGlow, 0.35f, 4.5f);
    addConeRoof(11.8f, 14.5f, 2.4f, 8, slateRoof, 0.72f);
    m_meshes[0].indexCount = static_cast<uint32_t>(indices.size()) - m_meshes[0].firstIndex;

    // 1: Timber Cabin / Outpost
    m_meshes[1].firstIndex = static_cast<uint32_t>(indices.size());
    glm::vec3 timber(0.28f, 0.16f, 0.09f);
    glm::vec3 darkWood(0.16f, 0.09f, 0.05f);
    addBox({-3.2f, -3.0f, -2.6f}, {3.2f, 0.5f, 2.6f}, darkStone, 0.90f, 0.0f);
    addBox({-2.8f, 0.5f, -2.2f}, {2.8f, 3.6f, 2.2f}, timber, 0.82f, 0.0f);
    addConeRoof(3.5f, 6.0f, 3.9f, 4, darkWood, 0.78f);
    addBox({1.3f, 3.6f, 0.8f}, {2.1f, 6.6f, 1.6f}, darkStone, 0.90f, 0.0f);
    addBox({-1.6f, 1.5f, 2.18f}, {-0.6f, 2.5f, 2.26f}, warmGlow, 0.25f, 3.2f);
    addBox({ 0.6f, 1.5f, 2.18f}, { 1.6f, 2.5f, 2.26f}, warmGlow, 0.25f, 3.2f);
    addBox({-2.86f, 1.5f, -0.5f}, {-2.78f, 2.5f, 0.5f}, warmGlow, 0.25f, 3.2f);
    m_meshes[1].indexCount = static_cast<uint32_t>(indices.size()) - m_meshes[1].firstIndex;

    // 2: Ancient Ruined Sandstone Colonnade & Arch
    m_meshes[2].firstIndex = static_cast<uint32_t>(indices.size());
    glm::vec3 sandstone(0.58f, 0.34f, 0.18f);
    glm::vec3 weathered(0.45f, 0.26f, 0.14f);
    addBox({-4.0f, -3.0f, -2.4f}, {4.0f, 0.6f, 2.4f}, weathered, 0.90f, 0.0f);
    addTaperedCyl(0.55f, 0.48f, 0.6f, 7.0f, 8, sandstone, 0.85f, 0.0f);
    addBox({-3.0f, 0.6f, -0.5f}, {-2.0f, 7.0f, 0.5f}, sandstone, 0.86f, 0.0f);
    addBox({ 2.0f, 0.6f, -0.5f}, { 3.0f, 7.0f, 0.5f}, sandstone, 0.86f, 0.0f);
    addBox({-3.4f, 7.0f, -0.7f}, { 3.4f, 8.2f, 0.7f}, weathered, 0.88f, 0.0f);
    addBox({-2.8f, 0.6f, 1.2f},  {-1.9f, 4.2f, 2.0f}, sandstone, 0.88f, 0.0f);
    m_meshes[2].indexCount = static_cast<uint32_t>(indices.size()) - m_meshes[2].firstIndex;

    // 3: Coastal Lighthouse
    m_meshes[3].firstIndex = static_cast<uint32_t>(indices.size());
    glm::vec3 whitePlaster(0.86f, 0.85f, 0.82f);
    glm::vec3 redStripe(0.68f, 0.12f, 0.10f);
    addTaperedCyl(3.0f, 2.8f, -3.5f, 0.8f, 10, darkStone, 0.90f, 0.0f);
    addTaperedCyl(2.2f, 1.85f, 0.8f, 5.5f, 10, whitePlaster, 0.75f, 0.0f);
    addTaperedCyl(1.85f, 1.55f, 5.5f, 9.5f, 10, redStripe, 0.75f, 0.0f);
    addTaperedCyl(1.55f, 1.30f, 9.5f, 13.5f, 10, whitePlaster, 0.75f, 0.0f);
    addTaperedCyl(1.75f, 1.75f, 13.5f, 14.2f, 10, darkStone, 0.65f, 0.0f);
    addTaperedCyl(1.05f, 1.05f, 14.2f, 16.0f, 10, glm::vec3(1.0f, 0.85f, 0.45f), 0.15f, 8.0f);
    addConeRoof(16.0f, 18.0f, 1.45f, 10, redStripe, 0.60f);
    m_meshes[3].indexCount = static_cast<uint32_t>(indices.size()) - m_meshes[3].firstIndex;

    // 4: Carved Standing Stone Monolith
    m_meshes[4].firstIndex = static_cast<uint32_t>(indices.size());
    glm::vec3 obsidian(0.10f, 0.11f, 0.14f);
    glm::vec3 runeCyan(0.20f, 0.78f, 0.98f);
    addTaperedCyl(2.5f, 2.2f, -3.0f, 0.7f, 6, darkStone, 0.88f, 0.0f);
    addTaperedCyl(1.1f, 0.65f, 0.7f, 8.5f, 4, obsidian, 0.45f, 0.0f);
    addConeRoof(8.5f, 10.2f, 0.65f, 4, obsidian, 0.45f);
    addBox({-0.25f, 3.2f, -0.92f}, {0.25f, 6.2f, 0.92f}, runeCyan, 0.25f, 4.2f);
    addBox({-0.92f, 4.2f, -0.25f}, {0.92f, 5.2f, 0.25f}, runeCyan, 0.25f, 4.2f);
    m_meshes[4].indexCount = static_cast<uint32_t>(indices.size()) - m_meshes[4].firstIndex;

    size_t vSize = sizeof(StructureVertex) * vertices.size();
    m_vertexBuffer = device.createBuffer(vSize, rhi::BufferUsage::Vertex | rhi::BufferUsage::TransferDst, rhi::MemoryUsage::GpuOnly);
    device.uploadBuffer(*m_vertexBuffer, vertices.data(), vSize);

    size_t iSize = sizeof(uint32_t) * indices.size();
    m_indexBuffer = device.createBuffer(iSize, rhi::BufferUsage::Index | rhi::BufferUsage::TransferDst, rhi::MemoryUsage::GpuOnly);
    device.uploadBuffer(*m_indexBuffer, indices.data(), iSize);
}

void StructureRenderer::createPipelines(rhi::RHIDevice& device) {
    std::vector<rhi::VertexBinding> bindings = {
        {0, sizeof(StructureVertex), rhi::VertexInputRate::Vertex},
        {1, sizeof(StructureInstance), rhi::VertexInputRate::Instance}
    };
    std::vector<rhi::VertexAttribute> attrs = {
        {0, 0, rhi::Format::RGB32F, static_cast<uint32_t>(offsetof(StructureVertex, pos))},
        {1, 0, rhi::Format::RGB32F, static_cast<uint32_t>(offsetof(StructureVertex, normal))},
        {2, 0, rhi::Format::RGB32F, static_cast<uint32_t>(offsetof(StructureVertex, color))},
        {3, 0, rhi::Format::RG32F,  static_cast<uint32_t>(offsetof(StructureVertex, matParams))},
        {4, 1, rhi::Format::RGBA32F, static_cast<uint32_t>(offsetof(StructureInstance, posScale))},
        {5, 1, rhi::Format::RGBA32F, static_cast<uint32_t>(offsetof(StructureInstance, params))}
    };

    rhi::GraphicsPipelineDesc desc{};
    desc.vertexShader = "structure.vert";
    desc.fragmentShader = "structure.frag";
    desc.vertexBindings = bindings;
    desc.vertexAttributes = attrs;
    desc.cullMode = rhi::CullMode::None;
    desc.depthTestEnable = true;
    desc.depthWriteEnable = true;
    desc.depthCompareOp = rhi::CompareOp::LessOrEqual;
    desc.blendEnable = false;
    desc.colorFormat = rhi::Format::RGBA16F;
    desc.depthFormat = rhi::Format::D32F;
    desc.hasDepth = true;
    desc.usesUniformBuffer = true;
    desc.usesStorageBuffer = false;
    desc.numSampledTextures = 1; // ShadowAtlas
    m_pipeline = device.createGraphicsPipeline(desc);

    rhi::GraphicsPipelineDesc sDesc{};
    sDesc.vertexShader = "shadow_structure.vert";
    sDesc.fragmentShader = "";
    sDesc.vertexBindings = bindings;
    sDesc.vertexAttributes = attrs;
    sDesc.cullMode = rhi::CullMode::None;
    sDesc.depthTestEnable = true;
    sDesc.depthWriteEnable = true;
    sDesc.depthCompareOp = rhi::CompareOp::LessOrEqual;
    sDesc.hasColor = false;
    sDesc.hasDepth = true;
    sDesc.depthFormat = rhi::Format::D32F;
    sDesc.depthBiasConstant = 1.5f;
    sDesc.depthBiasSlope = 2.0f;
    sDesc.usesUniformBuffer = true;
    sDesc.usesStorageBuffer = false;
    sDesc.pushConstantSize = sizeof(ShadowPushConstants);
    sDesc.pushConstantStages = rhi::ShaderStage::Vertex;
    m_shadowPipeline = device.createGraphicsPipeline(sDesc);
}

void StructureRenderer::generateForChunk(
    glm::vec2 chunkOrigin,
    const TerrainConfig& config,
    std::vector<StructureInstance>& outInstances
) {
    if (!config.showStructures || config.structureDensity <= 0.01f) return;

    uint32_t chunkSeed = static_cast<uint32_t>(std::abs(chunkOrigin.x * 49979687.0f + chunkOrigin.y * 86028121.0f)) + config.seed + 7717u;
    std::mt19937 gen(chunkSeed);
    std::uniform_real_distribution<float> distPos(14.0f, CHUNK_SIZE - 14.0f);
    std::uniform_real_distribution<float> distProb(0.0f, 1.0f);
    std::uniform_real_distribution<float> distRot(0.0f, 6.28318f);
    std::uniform_real_distribution<float> distScale(0.90f, 1.35f);
    std::uniform_real_distribution<float> distTint(-0.2f, 0.2f);

    // Average ~0.45 structures per chunk at density=1.0
    int attempts = std::max(1, static_cast<int>(std::round(2.0f * config.structureDensity)));
    for (int a = 0; a < attempts; ++a) {
        if (distProb(gen) > 0.26f * config.structureDensity) continue;

        glm::vec2 worldXZ = chunkOrigin + glm::vec2(distPos(gen), distPos(gen));
        float h = TerrainNoiseCPU::sampleHeight(worldXZ, config);
        if (h < config.waterHeight + 1.2f) continue;

        glm::vec3 n = TerrainNoiseCPU::sampleNormal(worldXZ, config);
        float slope = 1.0f - glm::clamp(n.y, 0.0f, 1.0f);
        if (slope > 0.24f) continue;

        uint32_t biome = TerrainNoiseCPU::sampleEffectiveBiome(worldXZ, config);
        float relH = (h - config.waterHeight) / std::max(config.amplitude, 15.0f);
        uint32_t type = 0;

        if (biome == 0u) { // Mountains -> Watchtower or Monolith
            type = (relH > 0.45f || distProb(gen) < 0.55f) ? 0u : 4u;
        } else if (biome == 1u) { // Hills -> Timber Cabin or Watchtower
            type = (distProb(gen) < 0.72f) ? 1u : 0u;
        } else if (biome == 2u) { // Canyons -> Sandstone Ruins or Monolith
            type = (distProb(gen) < 0.75f) ? 2u : 4u;
        } else { // Islands -> Lighthouse near coast or Ruins/Monolith
            type = (relH < 0.25f || distProb(gen) < 0.65f) ? 3u : 2u;
        }

        StructureInstance inst{};
        inst.posScale = glm::vec4(worldXZ.x, h - 0.15f, worldXZ.y, distScale(gen));
        inst.params = glm::vec4(distRot(gen), static_cast<float>(type), distTint(gen), 0.0f);
        outInstances.push_back(inst);
    }
}

void StructureRenderer::updateInstances(
    rhi::RHIDevice& device,
    const std::vector<glm::vec2>& chunkOrigins,
    const TerrainConfig& config
) {
    for (size_t i = 0; i < NUM_STRUCTURES; ++i) {
        m_instancesByType[i].clear();
    }

    if (!config.showStructures || config.structureDensity <= 0.01f) {
        m_instanceCounts.fill(0);
        return;
    }

    std::vector<StructureInstance> all;
    all.reserve(chunkOrigins.size() * 2);
    for (const auto& origin : chunkOrigins) {
        generateForChunk(origin, config, all);
    }

    for (const auto& inst : all) {
        uint32_t t = static_cast<uint32_t>(inst.params.y);
        if (t < NUM_STRUCTURES) {
            m_instancesByType[t].push_back(inst);
        }
    }

    for (uint32_t type = 0; type < NUM_STRUCTURES; ++type) {
        uint32_t count = static_cast<uint32_t>(m_instancesByType[type].size());
        m_instanceCounts[type] = count;
        if (count == 0) continue;

        size_t bufferSize = sizeof(StructureInstance) * count;
        for (uint32_t f = 0; f < MAX_FRAMES_IN_FLIGHT; ++f) {
            auto& buf = m_instanceBuffers[type][f];
            if (!buf || buf->getSize() < bufferSize) {
                buf = device.createBuffer(bufferSize, rhi::BufferUsage::Vertex, rhi::MemoryUsage::CpuToGpu);
            }
            buf->copyFromHost(m_instancesByType[type].data(), bufferSize);
        }
    }
}

void StructureRenderer::recordRenderCommands(const FrameContext& frame) {
    bool hasAny = false;
    for (uint32_t c : m_instanceCounts) {
        if (c > 0) { hasAny = true; break; }
    }
    if (!hasAny) return;

    rhi::RHICommandList& cmd = *frame.cmd;
    uint32_t frameIndex = frame.frameIndex;

    cmd.bindGraphicsPipeline(*m_pipeline);
    cmd.bindUniformBuffer(0, *frame.globalUBO);
    if (frame.shadows) {
        cmd.bindTexture(0, frame.shadows->getShadowAtlas(), frame.shadows->getShadowSampler());
    }
    cmd.bindIndexBuffer(*m_indexBuffer);
    cmd.bindVertexBuffer(0, *m_vertexBuffer);

    for (uint32_t type = 0; type < NUM_STRUCTURES; ++type) {
        if (m_instanceCounts[type] == 0) continue;
        cmd.bindVertexBuffer(1, *m_instanceBuffers[type][frameIndex]);
        const auto& mesh = m_meshes[type];
        cmd.drawIndexed(mesh.indexCount, m_instanceCounts[type], mesh.firstIndex);
    }
}

void StructureRenderer::recordShadowCommands(rhi::RHICommandList& cmd, const rhi::RHIBuffer& globalUBO, uint32_t frameIndex, uint32_t cascadeIndex) {
    bool hasAny = false;
    for (uint32_t c : m_instanceCounts) {
        if (c > 0) { hasAny = true; break; }
    }
    if (!hasAny) return;

    cmd.bindGraphicsPipeline(*m_shadowPipeline);
    cmd.bindUniformBuffer(0, globalUBO);

    ShadowPushConstants pc{};
    pc.cascadeInfo = glm::uvec4(cascadeIndex, 0u, 0u, 0u);
    cmd.pushConstants(rhi::ShaderStage::Vertex, &pc, sizeof(ShadowPushConstants));

    cmd.bindIndexBuffer(*m_indexBuffer);
    cmd.bindVertexBuffer(0, *m_vertexBuffer);

    for (uint32_t type = 0; type < NUM_STRUCTURES; ++type) {
        if (m_instanceCounts[type] == 0) continue;
        cmd.bindVertexBuffer(1, *m_instanceBuffers[type][frameIndex]);
        const auto& mesh = m_meshes[type];
        cmd.drawIndexed(mesh.indexCount, m_instanceCounts[type], mesh.firstIndex);
    }
}
