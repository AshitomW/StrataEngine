#include "renderer/FoliageRenderer.hpp"
#include "renderer/ShadowRenderer.hpp"
#include "terrain/TerrainNoiseCPU.hpp"
#include <cmath>
#include <cstring>
#include <random>

FoliageRenderer::FoliageRenderer(rhi::RHIDevice& device) {
    createModels(device);
    createPipeline(device);

    m_instancesByType.resize(NUM_MODELS);
}

void FoliageRenderer::createModels(rhi::RHIDevice& device) {
    std::vector<FoliageVertex> vertices;
    std::vector<uint32_t> indices;
    m_modelMeshes.resize(NUM_MODELS);

    auto addCylinder = [&](float radius, float minY, float maxY, int sides, glm::vec3 col) {
        uint32_t base = static_cast<uint32_t>(vertices.size());
        for (int i = 0; i < sides; ++i) {
            float a = static_cast<float>(i) / static_cast<float>(sides) * 2.0f * 3.14159f;
            float x = std::cos(a) * radius;
            float z = std::sin(a) * radius;
            glm::vec3 n(std::cos(a), 0.0f, std::sin(a));
            vertices.push_back({glm::vec3(x, minY, z), n, col});
            vertices.push_back({glm::vec3(x, maxY, z), n, col});
        }
        for (int i = 0; i < sides; ++i) {
            int next = (i + 1) % sides;
            indices.push_back(base + i * 2); indices.push_back(base + i * 2 + 1); indices.push_back(base + next * 2);
            indices.push_back(base + next * 2); indices.push_back(base + i * 2 + 1); indices.push_back(base + next * 2 + 1);
        }
    };

    auto addCone = [&](float baseY, float topY, float radius, int sides, glm::vec3 col) {
        uint32_t tipIdx = static_cast<uint32_t>(vertices.size());
        vertices.push_back({glm::vec3(0.0f, topY, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), col});
        uint32_t base = static_cast<uint32_t>(vertices.size());

        for (int i = 0; i < sides; ++i) {
            float a = static_cast<float>(i) / static_cast<float>(sides) * 2.0f * 3.14159f;
            float x = std::cos(a) * radius;
            float z = std::sin(a) * radius;
            glm::vec3 n = glm::normalize(glm::vec3(std::cos(a), 0.4f, std::sin(a)));
            vertices.push_back({glm::vec3(x, baseY, z), n, col});
        }
        for (int i = 0; i < sides; ++i) {
            int next = (i + 1) % sides;
            indices.push_back(tipIdx);
            indices.push_back(base + i);
            indices.push_back(base + next);
        }
    };

    m_modelMeshes[0].firstIndex = static_cast<uint32_t>(indices.size());
    addCylinder(0.38f, -1.2f, 3.2f, 6, glm::vec3(0.18f, 0.12f, 0.08f));
    addCone(1.8f, 5.5f, 2.4f, 7, glm::vec3(0.03f, 0.12f, 0.04f));
    addCone(4.2f, 8.0f, 1.8f, 7, glm::vec3(0.04f, 0.14f, 0.05f));
    addCone(6.5f, 10.5f, 1.2f, 7, glm::vec3(0.05f, 0.16f, 0.06f));
    m_modelMeshes[0].indexCount = static_cast<uint32_t>(indices.size()) - m_modelMeshes[0].firstIndex;

    m_modelMeshes[1].firstIndex = static_cast<uint32_t>(indices.size());
    addCylinder(0.48f, -1.2f, 4.0f, 6, glm::vec3(0.20f, 0.14f, 0.09f));
    addCone(3.2f, 7.5f, 2.8f, 8, glm::vec3(0.08f, 0.22f, 0.06f));
    addCone(5.0f, 8.8f, 2.2f, 8, glm::vec3(0.10f, 0.25f, 0.07f));
    m_modelMeshes[1].indexCount = static_cast<uint32_t>(indices.size()) - m_modelMeshes[1].firstIndex;

    m_modelMeshes[2].firstIndex = static_cast<uint32_t>(indices.size());
    addCylinder(0.32f, -1.2f, 6.2f, 6, glm::vec3(0.32f, 0.24f, 0.16f));
    for (int p = 0; p < 6; ++p) {
        float ang = static_cast<float>(p) / 6.0f * 2.0f * 3.14159f;
        float dx = std::cos(ang) * 3.2f;
        float dz = std::sin(ang) * 3.2f;
        uint32_t pBase = static_cast<uint32_t>(vertices.size());
        glm::vec3 frondCol(0.08f, 0.26f, 0.08f);
        vertices.push_back({glm::vec3(0.0f, 6.2f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), frondCol});
        vertices.push_back({glm::vec3(dx * 0.5f, 6.6f, dz * 0.5f), glm::vec3(0.0f, 1.0f, 0.0f), frondCol});
        vertices.push_back({glm::vec3(dx, 4.8f, dz), glm::vec3(0.0f, 1.0f, 0.0f), frondCol});
        indices.push_back(pBase); indices.push_back(pBase + 1); indices.push_back(pBase + 2);
    }
    m_modelMeshes[2].indexCount = static_cast<uint32_t>(indices.size()) - m_modelMeshes[2].firstIndex;

    m_modelMeshes[3].firstIndex = static_cast<uint32_t>(indices.size());
    glm::vec3 cactusCol(0.14f, 0.26f, 0.12f);
    addCylinder(0.42f, -1.0f, 5.2f, 6, cactusCol);
    addCylinder(0.26f, 2.0f, 4.2f, 5, cactusCol);
    m_modelMeshes[3].indexCount = static_cast<uint32_t>(indices.size()) - m_modelMeshes[3].firstIndex;

    m_modelMeshes[4].firstIndex = static_cast<uint32_t>(indices.size());
    uint32_t bBase = static_cast<uint32_t>(vertices.size());
    glm::vec3 rockCol(0.14f, 0.15f, 0.18f);
    vertices.push_back({glm::vec3( 0.0f,  1.4f,  0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), rockCol * 1.15f});
    vertices.push_back({glm::vec3(-1.2f,  0.3f, -0.9f), glm::vec3(-0.7f, 0.5f, -0.5f), rockCol});
    vertices.push_back({glm::vec3( 1.1f,  0.4f, -1.0f), glm::vec3( 0.7f, 0.5f, -0.5f), rockCol * 0.9f});
    vertices.push_back({glm::vec3( 1.3f,  0.2f,  0.9f), glm::vec3( 0.7f, 0.5f,  0.5f), rockCol});
    vertices.push_back({glm::vec3(-1.0f,  0.5f,  1.1f), glm::vec3(-0.6f, 0.5f,  0.6f), rockCol * 1.05f});
    vertices.push_back({glm::vec3( 0.0f, -0.8f,  0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), rockCol * 0.7f});

    indices.push_back(bBase + 0); indices.push_back(bBase + 1); indices.push_back(bBase + 2);
    indices.push_back(bBase + 0); indices.push_back(bBase + 2); indices.push_back(bBase + 3);
    indices.push_back(bBase + 0); indices.push_back(bBase + 3); indices.push_back(bBase + 4);
    indices.push_back(bBase + 0); indices.push_back(bBase + 4); indices.push_back(bBase + 1);
    indices.push_back(bBase + 5); indices.push_back(bBase + 2); indices.push_back(bBase + 1);
    indices.push_back(bBase + 5); indices.push_back(bBase + 3); indices.push_back(bBase + 2);
    indices.push_back(bBase + 5); indices.push_back(bBase + 4); indices.push_back(bBase + 3);
    indices.push_back(bBase + 5); indices.push_back(bBase + 1); indices.push_back(bBase + 4);
    m_modelMeshes[4].indexCount = static_cast<uint32_t>(indices.size()) - m_modelMeshes[4].firstIndex;

    m_modelMeshes[5].firstIndex = static_cast<uint32_t>(indices.size());
    uint32_t cBase = static_cast<uint32_t>(vertices.size());
    glm::vec3 sandstoneCol(0.48f, 0.20f, 0.10f);
    vertices.push_back({glm::vec3( 0.0f,  1.2f,  0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), sandstoneCol * 1.15f});
    vertices.push_back({glm::vec3(-1.0f,  0.3f, -0.8f), glm::vec3(-0.7f, 0.5f, -0.5f), sandstoneCol});
    vertices.push_back({glm::vec3( 1.0f,  0.4f, -0.8f), glm::vec3( 0.7f, 0.5f, -0.5f), sandstoneCol * 0.9f});
    vertices.push_back({glm::vec3( 1.1f,  0.2f,  0.8f), glm::vec3( 0.7f, 0.5f,  0.5f), sandstoneCol});
    vertices.push_back({glm::vec3(-0.9f,  0.5f,  0.9f), glm::vec3(-0.6f, 0.5f,  0.6f), sandstoneCol * 1.05f});
    vertices.push_back({glm::vec3( 0.0f, -0.8f,  0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), sandstoneCol * 0.7f});

    indices.push_back(cBase + 0); indices.push_back(cBase + 1); indices.push_back(cBase + 2);
    indices.push_back(cBase + 0); indices.push_back(cBase + 2); indices.push_back(cBase + 3);
    indices.push_back(cBase + 0); indices.push_back(cBase + 3); indices.push_back(cBase + 4);
    indices.push_back(cBase + 0); indices.push_back(cBase + 4); indices.push_back(cBase + 1);
    indices.push_back(cBase + 5); indices.push_back(cBase + 2); indices.push_back(cBase + 1);
    indices.push_back(cBase + 5); indices.push_back(cBase + 3); indices.push_back(cBase + 2);
    indices.push_back(cBase + 5); indices.push_back(cBase + 4); indices.push_back(cBase + 3);
    indices.push_back(cBase + 5); indices.push_back(cBase + 1); indices.push_back(cBase + 4);
    m_modelMeshes[5].indexCount = static_cast<uint32_t>(indices.size()) - m_modelMeshes[5].firstIndex;

    m_modelMeshes[6].firstIndex = static_cast<uint32_t>(indices.size());
    glm::vec3 gColBottom(0.06f, 0.16f, 0.04f);
    glm::vec3 gColTop(0.12f, 0.28f, 0.08f);

    float gWidth = 1.0f;
    float gHeight = 1.4f;
    for (int q = 0; q < 3; ++q) {
        float angle = static_cast<float>(q) * (3.14159f / 3.0f);
        float dx = std::cos(angle) * gWidth;
        float dz = std::sin(angle) * gWidth;
        uint32_t qBase = static_cast<uint32_t>(vertices.size());
        glm::vec3 n(std::sin(angle), 0.2f, -std::cos(angle));

        vertices.push_back({glm::vec3(-dx, -0.5f, -dz), n, gColBottom});
        vertices.push_back({glm::vec3( dx, -0.5f,  dz), n, gColBottom});
        vertices.push_back({glm::vec3(-dx * 0.8f, gHeight, -dz * 0.8f), n, gColTop});
        vertices.push_back({glm::vec3( dx * 0.8f, gHeight,  dz * 0.8f), n, gColTop});

        indices.push_back(qBase + 0); indices.push_back(qBase + 2); indices.push_back(qBase + 1);
        indices.push_back(qBase + 1); indices.push_back(qBase + 2); indices.push_back(qBase + 3);
    }
    m_modelMeshes[6].indexCount = static_cast<uint32_t>(indices.size()) - m_modelMeshes[6].firstIndex;

    size_t vSize = sizeof(FoliageVertex) * vertices.size();
    m_vertexBuffer = device.createBuffer(vSize, rhi::BufferUsage::Vertex | rhi::BufferUsage::TransferDst, rhi::MemoryUsage::GpuOnly);
    device.uploadBuffer(*m_vertexBuffer, vertices.data(), vSize);

    size_t iSize = sizeof(uint32_t) * indices.size();
    m_indexBuffer = device.createBuffer(iSize, rhi::BufferUsage::Index | rhi::BufferUsage::TransferDst, rhi::MemoryUsage::GpuOnly);
    device.uploadBuffer(*m_indexBuffer, indices.data(), iSize);
}

void FoliageRenderer::createPipeline(rhi::RHIDevice& device) {
    rhi::GraphicsPipelineDesc desc{};
    desc.vertexShader = "foliage.vert";
    desc.fragmentShader = "foliage.frag";
    desc.vertexBindings = {
        {0, sizeof(FoliageVertex), rhi::VertexInputRate::Vertex},
        {1, sizeof(FoliageInstance), rhi::VertexInputRate::Instance}
    };
    desc.vertexAttributes = {
        {0, 0, rhi::Format::RGB32F, static_cast<uint32_t>(offsetof(FoliageVertex, pos))},
        {1, 0, rhi::Format::RGB32F, static_cast<uint32_t>(offsetof(FoliageVertex, normal))},
        {2, 0, rhi::Format::RGB32F, static_cast<uint32_t>(offsetof(FoliageVertex, color))},
        {3, 1, rhi::Format::RGBA32F, static_cast<uint32_t>(offsetof(FoliageInstance, posScale))},
        {4, 1, rhi::Format::RGBA32F, static_cast<uint32_t>(offsetof(FoliageInstance, params))}
    };
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
    desc.numSampledTextures = 1;

    m_pipeline = device.createGraphicsPipeline(desc);
}

float FoliageRenderer::sampleExactHeight(glm::vec2 worldPos, const TerrainConfig& config) const {
    return TerrainNoiseCPU::sampleHeight(worldPos, config);
}

glm::vec3 FoliageRenderer::sampleExactNormal(glm::vec2 worldPos, const TerrainConfig& config, float) const {
    return TerrainNoiseCPU::sampleNormal(worldPos, config);
}

void FoliageRenderer::generateScatterForChunk(
    glm::vec2 chunkOrigin,
    const TerrainConfig& config,
    std::vector<FoliageInstance>& outInstances
) {
    if (!config.showFoliage || config.foliageDensity <= 0.001f) return;

    uint32_t chunkSeed = static_cast<uint32_t>(std::abs(chunkOrigin.x * 73856093.0f + chunkOrigin.y * 19349663.0f)) + config.seed;
    std::mt19937 gen(chunkSeed);
    std::uniform_real_distribution<float> distOffset(3.0f, CHUNK_SIZE - 3.0f);
    std::uniform_real_distribution<float> distRot(0.0f, 6.28318f);
    std::uniform_real_distribution<float> distScale(0.85f, 1.25f);
    std::uniform_real_distribution<float> distVar(-0.25f, 0.25f);
    std::uniform_real_distribution<float> distProb(0.0f, 1.0f);

    int numCandidates = static_cast<int>(50.0f * config.foliageDensity);

    for (int i = 0; i < numCandidates; ++i) {
        float localX = distOffset(gen);
        float localZ = distOffset(gen);
        glm::vec2 worldXZ = chunkOrigin + glm::vec2(localX, localZ);

        float height = sampleExactHeight(worldXZ, config);

        if (height < config.waterHeight + 0.5f) {
            continue;
        }

        glm::vec3 normal = sampleExactNormal(worldXZ, config, height);
        float slope = 1.0f - glm::clamp(normal.y, 0.0f, 1.0f);

        float relH = (height - config.waterHeight) / config.amplitude;

        uint32_t effectiveBiome = TerrainNoiseCPU::sampleEffectiveBiome(worldXZ, config);

        uint32_t objType = 0;
        float prob = distProb(gen);
        float scale = distScale(gen);

        if (effectiveBiome == 0u) {

            if (relH > 0.70f) {
                objType = 4;
                if (prob < 0.60f) continue;
            } else if (slope > 0.45f) {
                objType = 4;
                if (prob < 0.50f) continue;
            } else if (relH < 0.25f && prob < 0.35f) {
                objType = 6;
                scale *= 1.3f;
            } else {
                objType = 0;
                if (prob < 0.25f) objType = 4;
            }

        } else if (effectiveBiome == 1u) {

            if (slope > 0.50f) {
                objType = 4;
                if (prob < 0.60f) continue;
            } else if (prob < 0.40f) {
                objType = 1;
            } else if (prob < 0.85f) {
                objType = 6;
                scale *= 1.35f;
            } else {
                objType = 4;
                scale *= 0.75f;
            }

        } else if (effectiveBiome == 2u) {

            if (slope > 0.40f) {
                objType = 5;
                if (prob < 0.50f) continue;
            } else if (prob < 0.45f) {
                objType = 3;
                scale *= 1.15f;
            } else if (prob < 0.80f) {
                objType = 5;
            } else {
                continue;
            }

        } else {

            if (slope > 0.50f) {
                objType = 4;
                if (prob < 0.60f) continue;
            } else if (relH < 0.15f || prob < 0.55f) {
                objType = 2;
                scale *= 1.25f;
            } else if (prob < 0.80f) {
                objType = 6;
                scale *= 1.4f;
            } else {
                objType = 4;
                scale *= 0.85f;
            }
        }

        FoliageInstance inst{};

        inst.posScale = glm::vec4(worldXZ.x, height - 0.15f, worldXZ.y, scale);
        inst.params = glm::vec4(distRot(gen), static_cast<float>(objType), distVar(gen), 0.0f);
        outInstances.push_back(inst);
    }
}

void FoliageRenderer::updateInstances(
    rhi::RHIDevice& device,
    const std::vector<glm::vec2>& chunkOrigins,
    const TerrainConfig& config
) {
    for (size_t i = 0; i < NUM_MODELS; ++i) {
        m_instancesByType[i].clear();
    }

    if (!config.showFoliage || config.foliageDensity <= 0.001f) {
        m_instanceCounts.fill(0);
        return;
    }

    std::vector<FoliageInstance> allInstances;
    allInstances.reserve(chunkOrigins.size() * static_cast<size_t>(50.0f * config.foliageDensity));

    for (const auto& origin : chunkOrigins) {
        generateScatterForChunk(origin, config, allInstances);
    }

    for (const auto& inst : allInstances) {
        uint32_t type = static_cast<uint32_t>(inst.params.y);
        if (type < NUM_MODELS) {
            m_instancesByType[type].push_back(inst);
        }
    }

    for (uint32_t type = 0; type < NUM_MODELS; ++type) {
        uint32_t count = static_cast<uint32_t>(m_instancesByType[type].size());
        m_instanceCounts[type] = count;
        if (count == 0) continue;

        size_t bufferSize = sizeof(FoliageInstance) * count;
        for (uint32_t f = 0; f < MAX_FRAMES_IN_FLIGHT; ++f) {
            auto& buffer = m_instanceBuffers[type][f];

            if (!buffer || buffer->getSize() < bufferSize) {
                buffer = device.createBuffer(bufferSize, rhi::BufferUsage::Vertex, rhi::MemoryUsage::CpuToGpu);
            }

            buffer->copyFromHost(m_instancesByType[type].data(), bufferSize);
        }
    }
}

void FoliageRenderer::recordRenderCommands(const FrameContext& frame) {
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

    for (uint32_t type = 0; type < NUM_MODELS; ++type) {
        if (m_instanceCounts[type] == 0) continue;

        cmd.bindVertexBuffer(1, *m_instanceBuffers[type][frameIndex]);

        const auto& mesh = m_modelMeshes[type];
        cmd.drawIndexed(mesh.indexCount, m_instanceCounts[type], mesh.firstIndex);
    }
}

void FoliageRenderer::recordShadowCommands(rhi::RHICommandList& cmd, uint32_t frameIndex, uint32_t cascadeIndex) {
    bool hasAny = false;
    for (uint32_t c : m_instanceCounts) {
        if (c > 0) { hasAny = true; break; }
    }
    if (!hasAny) return;

    ShadowPushConstants pc{};
    pc.cascadeInfo = glm::uvec4(cascadeIndex, 0u, 0u, 0u);
    cmd.pushConstants(rhi::ShaderStage::Vertex, &pc, sizeof(ShadowPushConstants));

    cmd.bindIndexBuffer(*m_indexBuffer);
    cmd.bindVertexBuffer(0, *m_vertexBuffer);

    for (uint32_t type = 0; type < NUM_MODELS; ++type) {
        if (m_instanceCounts[type] == 0) continue;

        cmd.bindVertexBuffer(1, *m_instanceBuffers[type][frameIndex]);
        const auto& mesh = m_modelMeshes[type];
        cmd.drawIndexed(mesh.indexCount, m_instanceCounts[type], mesh.firstIndex);
    }
}