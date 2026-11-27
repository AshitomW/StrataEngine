#include "renderer/FoliageRenderer.hpp"
#include "renderer/ShadowRenderer.hpp"
#include "terrain/TerrainNoiseCPU.hpp"
#include <cmath>
#include <cstring>
#include <random>
#include <unordered_set>

FoliageRenderer::FoliageRenderer(rhi::RHIDevice& device) {
    createModels(device);
    createPipeline(device);

    m_instancesByType.resize(NUM_MODELS);
}

void FoliageRenderer::createModels(rhi::RHIDevice& device) {
    std::vector<FoliageVertex> vertices;
    std::vector<uint32_t> indices;
    m_modelMeshes.resize(NUM_MODELS);

    auto addTapered = [&](float rBot, float rTop, float minY, float maxY, float offX, float offZ, int sides, glm::vec3 col) {
        uint32_t base = static_cast<uint32_t>(vertices.size());
        for (int i = 0; i < sides; ++i) {
            float a = static_cast<float>(i) / static_cast<float>(sides) * 2.0f * 3.14159f;
            glm::vec3 n(std::cos(a), 0.1f, std::sin(a));
            vertices.push_back({glm::vec3(std::cos(a) * rBot, minY, std::sin(a) * rBot), glm::normalize(n), col * 0.85f});
            vertices.push_back({glm::vec3(offX + std::cos(a) * rTop, maxY, offZ + std::sin(a) * rTop), glm::normalize(n), col});
        }
        for (int i = 0; i < sides; ++i) {
            int next = (i + 1) % sides;
            indices.push_back(base + i * 2); indices.push_back(base + i * 2 + 1); indices.push_back(base + next * 2);
            indices.push_back(base + next * 2); indices.push_back(base + i * 2 + 1); indices.push_back(base + next * 2 + 1);
        }
    };

    auto addCone = [&](float baseY, float topY, float radius, float offX, float offZ, int sides, glm::vec3 baseCol, glm::vec3 tipCol) {
        uint32_t tipIdx = static_cast<uint32_t>(vertices.size());
        vertices.push_back({glm::vec3(offX, topY, offZ), glm::vec3(0.0f, 1.0f, 0.0f), tipCol});
        uint32_t base = static_cast<uint32_t>(vertices.size());

        for (int i = 0; i < sides; ++i) {
            float a = static_cast<float>(i) / static_cast<float>(sides) * 2.0f * 3.14159f;
            float x = offX + std::cos(a) * radius;
            float z = offZ + std::sin(a) * radius;
            glm::vec3 n = glm::normalize(glm::vec3(std::cos(a), 0.45f, std::sin(a)));
            vertices.push_back({glm::vec3(x, baseY, z), n, baseCol});
        }
        for (int i = 0; i < sides; ++i) {
            int next = (i + 1) % sides;
            indices.push_back(tipIdx);
            indices.push_back(base + i);
            indices.push_back(base + next);
        }
    };

    auto addRock = [&](float rx, float ry, float rz, glm::vec3 col) {
        uint32_t base = static_cast<uint32_t>(vertices.size());
        const int rings = 3;
        const int segs = 7;
        for (int r = 0; r <= rings; ++r) {
            float v = static_cast<float>(r) / static_cast<float>(rings);
            float phi = v * 3.14159f;
            for (int s = 0; s < segs; ++s) {
                float u = static_cast<float>(s) / static_cast<float>(segs);
                float theta = u * 2.0f * 3.14159f;
                float jag = 0.85f + 0.28f * std::sin(theta * 3.0f + phi * 2.0f);
                glm::vec3 dir(std::sin(phi) * std::cos(theta), std::cos(phi), std::sin(phi) * std::sin(theta));
                glm::vec3 pos(dir.x * rx * jag, dir.y * ry * jag + 0.15f * ry, dir.z * rz * jag);
                float shade = 0.72f + 0.38f * std::max(0.0f, dir.y);
                vertices.push_back({pos, glm::normalize(dir), col * shade});
            }
        }
        for (int r = 0; r < rings; ++r) {
            for (int s = 0; s < segs; ++s) {
                int ns = (s + 1) % segs;
                uint32_t a = base + r * segs + s;
                uint32_t b = base + r * segs + ns;
                uint32_t c = base + (r + 1) * segs + s;
                uint32_t d = base + (r + 1) * segs + ns;
                indices.push_back(a); indices.push_back(c); indices.push_back(b);
                indices.push_back(b); indices.push_back(c); indices.push_back(d);
            }
        }
    };

    // 0: Layered Alpine Pine (5 tiers)
    m_modelMeshes[0].firstIndex = static_cast<uint32_t>(indices.size());
    addTapered(0.45f, 0.16f, -1.2f, 5.2f, 0.0f, 0.0f, 7, glm::vec3(0.20f, 0.13f, 0.08f));
    addCone(1.5f, 4.6f, 2.7f, 0.0f, 0.0f, 8, glm::vec3(0.025f, 0.10f, 0.035f), glm::vec3(0.045f, 0.16f, 0.055f));
    addCone(3.0f, 6.2f, 2.3f, 0.0f, 0.0f, 8, glm::vec3(0.030f, 0.12f, 0.040f), glm::vec3(0.055f, 0.18f, 0.060f));
    addCone(4.5f, 7.8f, 1.8f, 0.0f, 0.0f, 8, glm::vec3(0.035f, 0.14f, 0.045f), glm::vec3(0.065f, 0.21f, 0.070f));
    addCone(6.1f, 9.4f, 1.3f, 0.0f, 0.0f, 7, glm::vec3(0.045f, 0.16f, 0.055f), glm::vec3(0.075f, 0.24f, 0.080f));
    addCone(7.6f, 11.2f, 0.85f, 0.0f, 0.0f, 7, glm::vec3(0.055f, 0.18f, 0.060f), glm::vec3(0.090f, 0.27f, 0.095f));
    m_modelMeshes[0].indexCount = static_cast<uint32_t>(indices.size()) - m_modelMeshes[0].firstIndex;

    // 1: Broadleaf Oak with multi-lobe crown
    m_modelMeshes[1].firstIndex = static_cast<uint32_t>(indices.size());
    addTapered(0.60f, 0.34f, -1.2f, 3.8f, 0.0f, 0.0f, 7, glm::vec3(0.22f, 0.14f, 0.09f));
    addTapered(0.24f, 0.14f, 2.8f, 5.4f, 1.1f, 0.6f, 5, glm::vec3(0.22f, 0.14f, 0.09f));
    addTapered(0.22f, 0.12f, 2.9f, 5.2f, -1.0f, -0.7f, 5, glm::vec3(0.22f, 0.14f, 0.09f));
    addCone(3.3f, 7.6f, 3.0f, 0.0f, 0.0f, 9, glm::vec3(0.06f, 0.18f, 0.04f), glm::vec3(0.13f, 0.30f, 0.08f));
    addCone(4.0f, 7.0f, 2.1f, 1.2f, 0.7f, 8, glm::vec3(0.07f, 0.20f, 0.05f), glm::vec3(0.15f, 0.33f, 0.09f));
    addCone(3.8f, 6.8f, 2.0f, -1.1f, -0.8f, 8, glm::vec3(0.06f, 0.19f, 0.04f), glm::vec3(0.14f, 0.31f, 0.08f));
    addCone(5.3f, 9.2f, 2.3f, 0.0f, 0.0f, 8, glm::vec3(0.09f, 0.24f, 0.06f), glm::vec3(0.17f, 0.36f, 0.10f));
    m_modelMeshes[1].indexCount = static_cast<uint32_t>(indices.size()) - m_modelMeshes[1].firstIndex;

    // 2: Curved Segmented Palm Tree
    m_modelMeshes[2].firstIndex = static_cast<uint32_t>(indices.size());
    addTapered(0.38f, 0.28f, -1.2f, 2.5f, 0.35f, 0.15f, 7, glm::vec3(0.30f, 0.22f, 0.14f));
    addTapered(0.28f, 0.20f, 2.4f, 6.4f, 0.75f, 0.30f, 7, glm::vec3(0.34f, 0.26f, 0.17f));
    for (int p = 0; p < 10; ++p) {
        float ang = static_cast<float>(p) / 10.0f * 2.0f * 3.14159f;
        float len = (p % 2 == 0) ? 3.5f : 2.8f;
        float lift = (p % 2 == 0) ? 6.8f : 7.2f;
        float droop = (p % 2 == 0) ? 4.6f : 5.4f;
        float dx = std::cos(ang) * len;
        float dz = std::sin(ang) * len;
        uint32_t pBase = static_cast<uint32_t>(vertices.size());
        glm::vec3 frondDark(0.05f, 0.19f, 0.05f);
        glm::vec3 frondBright(0.12f, 0.32f, 0.09f);
        vertices.push_back({glm::vec3(0.75f, 6.4f, 0.30f), glm::vec3(0.0f, 1.0f, 0.0f), frondDark});
        vertices.push_back({glm::vec3(0.75f + dx * 0.45f, lift, 0.30f + dz * 0.45f), glm::vec3(0.0f, 1.0f, 0.0f), frondBright});
        vertices.push_back({glm::vec3(0.75f + dx, droop, 0.30f + dz), glm::vec3(0.0f, 0.7f, 0.0f), frondDark});
        indices.push_back(pBase); indices.push_back(pBase + 1); indices.push_back(pBase + 2);
    }
    m_modelMeshes[2].indexCount = static_cast<uint32_t>(indices.size()) - m_modelMeshes[2].firstIndex;

    // 3: Ribbed Desert Saguaro Cactus
    m_modelMeshes[3].firstIndex = static_cast<uint32_t>(indices.size());
    glm::vec3 cactusCol(0.14f, 0.28f, 0.11f);
    addTapered(0.42f, 0.36f, -1.0f, 5.4f, 0.0f, 0.0f, 7, cactusCol);
    addTapered(0.24f, 0.20f, 2.0f, 4.3f, 0.95f, 0.0f, 6, cactusCol * 1.08f);
    addTapered(0.22f, 0.18f, 2.5f, 4.7f, -0.85f, 0.15f, 6, cactusCol * 1.05f);
    m_modelMeshes[3].indexCount = static_cast<uint32_t>(indices.size()) - m_modelMeshes[3].firstIndex;

    // 4: Granite Boulder
    m_modelMeshes[4].firstIndex = static_cast<uint32_t>(indices.size());
    addRock(1.35f, 0.95f, 1.25f, glm::vec3(0.16f, 0.17f, 0.20f));
    m_modelMeshes[4].indexCount = static_cast<uint32_t>(indices.size()) - m_modelMeshes[4].firstIndex;

    // 5: Red Sandstone Boulder
    m_modelMeshes[5].firstIndex = static_cast<uint32_t>(indices.size());
    addRock(1.25f, 0.85f, 1.15f, glm::vec3(0.46f, 0.22f, 0.11f));
    m_modelMeshes[5].indexCount = static_cast<uint32_t>(indices.size()) - m_modelMeshes[5].firstIndex;

    // 6: Wild Grass Clump (4 crossed blades)
    m_modelMeshes[6].firstIndex = static_cast<uint32_t>(indices.size());
    glm::vec3 gColBottom(0.06f, 0.16f, 0.04f);
    glm::vec3 gColTop(0.14f, 0.30f, 0.09f);
    float gWidth = 0.95f;
    float gHeight = 1.45f;
    for (int q = 0; q < 4; ++q) {
        float angle = static_cast<float>(q) * (3.14159f / 4.0f);
        float dx = std::cos(angle) * gWidth;
        float dz = std::sin(angle) * gWidth;
        uint32_t qBase = static_cast<uint32_t>(vertices.size());
        glm::vec3 n(std::sin(angle), 0.2f, -std::cos(angle));
        vertices.push_back({glm::vec3(-dx, -0.4f, -dz), n, gColBottom});
        vertices.push_back({glm::vec3( dx, -0.4f,  dz), n, gColBottom});
        vertices.push_back({glm::vec3(-dx * 0.85f + 0.12f, gHeight, -dz * 0.85f), n, gColTop});
        vertices.push_back({glm::vec3( dx * 0.85f + 0.12f, gHeight,  dz * 0.85f), n, gColTop});
        indices.push_back(qBase + 0); indices.push_back(qBase + 2); indices.push_back(qBase + 1);
        indices.push_back(qBase + 1); indices.push_back(qBase + 2); indices.push_back(qBase + 3);
    }
    m_modelMeshes[6].indexCount = static_cast<uint32_t>(indices.size()) - m_modelMeshes[6].firstIndex;

    // 7: Tall Segmented Bamboo Cluster
    m_modelMeshes[7].firstIndex = static_cast<uint32_t>(indices.size());
    glm::vec3 bambooStalk(0.20f, 0.34f, 0.09f);
    glm::vec3 bambooLeaf(0.08f, 0.28f, 0.06f);
    glm::vec3 bambooTip(0.16f, 0.38f, 0.10f);
    const float culmOffsets[5][4] = {
        { 0.0f,  0.0f,  0.35f,  0.20f},
        { 0.65f, 0.30f, 0.70f, -0.15f},
        {-0.55f, 0.45f,-0.45f,  0.55f},
        { 0.25f,-0.60f,-0.15f, -0.70f},
        {-0.40f,-0.45f, 0.20f, -0.50f}
    };
    const float culmHeights[5] = { 10.5f, 9.2f, 8.6f, 9.8f, 7.8f };
    for (int c = 0; c < 5; ++c) {
        float bx = culmOffsets[c][0], bz = culmOffsets[c][1];
        float tx = culmOffsets[c][2], tz = culmOffsets[c][3];
        float h = culmHeights[c];
        // Build vertical segmented culm from (bx, 0, bz) to (tx, h, tz)
        uint32_t cBase = static_cast<uint32_t>(vertices.size());
        const int sides = 5;
        for (int i = 0; i < sides; ++i) {
            float a = static_cast<float>(i) / static_cast<float>(sides) * 2.0f * 3.14159f;
            glm::vec3 n(std::cos(a), 0.1f, std::sin(a));
            vertices.push_back({glm::vec3(bx + std::cos(a) * 0.13f, -0.6f, bz + std::sin(a) * 0.13f), glm::normalize(n), bambooStalk * 0.8f});
            vertices.push_back({glm::vec3(tx + std::cos(a) * 0.08f, h, tz + std::sin(a) * 0.08f), glm::normalize(n), bambooStalk});
        }
        for (int i = 0; i < sides; ++i) {
            int next = (i + 1) % sides;
            indices.push_back(cBase + i * 2); indices.push_back(cBase + i * 2 + 1); indices.push_back(cBase + next * 2);
            indices.push_back(cBase + next * 2); indices.push_back(cBase + i * 2 + 1); indices.push_back(cBase + next * 2 + 1);
        }
        addCone(h * 0.65f, h + 1.1f, 1.35f, tx, tz, 6, bambooLeaf, bambooTip);
        addCone(h * 0.48f, h * 0.85f, 1.15f, (bx + tx) * 0.5f, (bz + tz) * 0.5f, 6, bambooLeaf * 0.9f, bambooTip);
    }
    m_modelMeshes[7].indexCount = static_cast<uint32_t>(indices.size()) - m_modelMeshes[7].firstIndex;

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
        float autumnFlag = 0.0f;

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

        } else if (effectiveBiome == 3u) {
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
        } else if (effectiveBiome == 5u) { // Alpine Emerald Valley
            if (relH > 0.52f || slope > 0.46f) {
                objType = 4;
                if (prob < 0.65f) continue;
            } else if (prob < 0.48f) {
                objType = 0; // Pine
                scale *= 1.12f;
            } else if (prob < 0.85f) {
                objType = 6; // Valley grass
                scale *= 1.35f;
            } else {
                objType = 4;
            }
        } else if (effectiveBiome == 6u) { // Arctic Tundra
            if (slope > 0.45f) {
                objType = 4;
                if (prob < 0.55f) continue;
            } else if (relH < 0.35f && prob < 0.25f) {
                objType = 0;
                scale *= 0.85f;
            } else if (prob < 0.60f) {
                objType = 4;
                scale *= 0.80f;
            } else {
                continue;
            }
        } else if (effectiveBiome == 7u) { // Autumn Highlands
            if (slope > 0.46f || relH > 0.68f) {
                objType = 4;
                if (prob < 0.55f) continue;
            } else if (prob < 0.44f) {
                objType = 1; // Autumn Broadleaf Oak
                autumnFlag = 1.0f;
                scale *= 1.10f;
            } else if (prob < 0.56f) {
                objType = 0; // Evergreen Pine contrast
            } else if (prob < 0.88f) {
                objType = 6; // Heather/grass
                autumnFlag = 1.0f;
                scale *= 1.30f;
            } else {
                objType = 4;
                scale *= 0.85f;
            }
        } else { // Karst Peaks & Bamboo (8u)
            if (slope > 0.44f || relH > 0.40f) {
                if (prob < 0.72f) continue;
                objType = 4;
                scale *= 0.85f;
            } else if (prob < 0.45f) {
                objType = 7; // Bamboo Cluster
                scale *= 1.10f;
            } else if (prob < 0.60f) {
                objType = 1; // Broadleaf
            } else if (prob < 0.88f) {
                objType = 6;
                scale *= 1.35f;
            } else {
                objType = 4;
            }
        }

        FoliageInstance inst{};

        inst.posScale = glm::vec4(worldXZ.x, height - 0.15f, worldXZ.y, scale);
        inst.params = glm::vec4(distRot(gen), static_cast<float>(objType), distVar(gen), autumnFlag);
        outInstances.push_back(inst);
    }
}

void FoliageRenderer::updateInstances(
    const std::vector<glm::vec2>& chunkOrigins,
    const TerrainConfig& config
) {
    for (size_t i = 0; i < NUM_MODELS; ++i) {
        m_instancesByType[i].clear();
    }

    ScatterKey scatterKey{
        config.seed, config.presetType, config.waterHeight, config.foliageDensity,
        config.showFoliage, config.amplitude, config.frequency, config.warpStrength, config.mountainPower
    };
    if (!(scatterKey == m_cachedScatterKey)) {
        m_chunkCache.clear();
        m_cachedScatterKey = scatterKey;
    }

    if (!config.showFoliage || config.foliageDensity <= 0.001f) {
        m_instanceCounts.fill(0);
        m_frameBufferDirty.fill(true);
        return;
    }

    auto packChunkKey = [](glm::vec2 origin) -> uint64_t {
        int32_t cx = static_cast<int32_t>(std::round(origin.x / CHUNK_SIZE));
        int32_t cz = static_cast<int32_t>(std::round(origin.y / CHUNK_SIZE));
        return (static_cast<uint64_t>(static_cast<uint32_t>(cx)) << 32) | static_cast<uint64_t>(static_cast<uint32_t>(cz));
    };

    std::unordered_set<uint64_t> activeKeys;
    activeKeys.reserve(chunkOrigins.size() * 2);

    for (const auto& origin : chunkOrigins) {
        uint64_t key = packChunkKey(origin);
        activeKeys.insert(key);
        auto it = m_chunkCache.find(key);
        if (it == m_chunkCache.end()) {
            std::vector<FoliageInstance> chunkInstances;
            generateScatterForChunk(origin, config, chunkInstances);
            it = m_chunkCache.emplace(key, std::move(chunkInstances)).first;
        }
        for (const auto& inst : it->second) {
            uint32_t type = static_cast<uint32_t>(inst.params.y);
            if (type < NUM_MODELS) {
                m_instancesByType[type].push_back(inst);
            }
        }
    }

    for (auto it = m_chunkCache.begin(); it != m_chunkCache.end(); ) {
        if (activeKeys.find(it->first) == activeKeys.end()) {
            it = m_chunkCache.erase(it);
        } else {
            ++it;
        }
    }

    for (uint32_t type = 0; type < NUM_MODELS; ++type) {
        m_instanceCounts[type] = static_cast<uint32_t>(m_instancesByType[type].size());
    }
    m_frameBufferDirty.fill(true);
}

void FoliageRenderer::ensureFrameBuffersUploaded(rhi::RHIDevice& device, uint32_t frameIndex) {
    if (frameIndex >= MAX_FRAMES_IN_FLIGHT || !m_frameBufferDirty[frameIndex]) {
        return;
    }

    for (uint32_t type = 0; type < NUM_MODELS; ++type) {
        uint32_t count = m_instanceCounts[type];
        if (count == 0) continue;

        size_t bufferSize = sizeof(FoliageInstance) * count;
        auto& buffer = m_instanceBuffers[type][frameIndex];
        if (!buffer || buffer->getSize() < bufferSize) {
            buffer = device.createBuffer(bufferSize, rhi::BufferUsage::Vertex, rhi::MemoryUsage::CpuToGpu);
        }
        buffer->copyFromHost(m_instancesByType[type].data(), bufferSize);
    }

    m_frameBufferDirty[frameIndex] = false;
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