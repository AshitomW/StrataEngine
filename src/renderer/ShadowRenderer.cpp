#include "renderer/ShadowRenderer.hpp"
#include <algorithm>
#include <cmath>

ShadowRenderer::ShadowRenderer(rhi::RHIDevice& device) {
    rhi::TextureDesc atlasDesc{};
    atlasDesc.width = ATLAS_RES;
    atlasDesc.height = ATLAS_RES;
    atlasDesc.format = rhi::Format::D32F;
    atlasDesc.usage = rhi::TextureUsage::DepthAttachment | rhi::TextureUsage::Sampled;
    m_shadowAtlas = device.createTexture(atlasDesc);

    rhi::SamplerDesc sampDesc{};
    sampDesc.minFilter = rhi::Filter::Nearest;
    sampDesc.magFilter = rhi::Filter::Nearest;
    sampDesc.addressU = rhi::AddressMode::ClampToEdge;
    sampDesc.addressV = rhi::AddressMode::ClampToEdge;
    sampDesc.addressW = rhi::AddressMode::ClampToEdge;
    m_shadowSampler = device.createSampler(sampDesc);

    device.executeImmediateCompute([&](rhi::RHICommandList& cmd) {
        cmd.textureBarrier(*m_shadowAtlas, rhi::TextureLayout::Undefined, rhi::TextureLayout::ShaderReadOnly);
    });

    rhi::GraphicsPipelineDesc tDesc{};
    tDesc.vertexShader = "shadow_terrain.vert";
    tDesc.fragmentShader = "";
    tDesc.cullMode = rhi::CullMode::None;
    tDesc.depthTestEnable = true;
    tDesc.depthWriteEnable = true;
    tDesc.depthCompareOp = rhi::CompareOp::LessOrEqual;
    tDesc.hasColor = false;
    tDesc.hasDepth = true;
    tDesc.depthFormat = rhi::Format::D32F;
    tDesc.depthBiasConstant = 1.5f;
    tDesc.depthBiasSlope = 2.0f;
    tDesc.usesUniformBuffer = true;
    tDesc.usesStorageBuffer = true;
    tDesc.pushConstantSize = sizeof(ChunkPushConstants);
    tDesc.pushConstantStages = rhi::ShaderStage::Vertex;
    m_terrainShadowPipeline = device.createGraphicsPipeline(tDesc);

    rhi::GraphicsPipelineDesc fDesc{};
    fDesc.vertexShader = "shadow_foliage.vert";
    fDesc.fragmentShader = "";
    fDesc.vertexBindings = {
        {0, sizeof(FoliageVertex), rhi::VertexInputRate::Vertex},
        {1, sizeof(FoliageInstance), rhi::VertexInputRate::Instance}
    };
    fDesc.vertexAttributes = {
        {0, 0, rhi::Format::RGB32F, static_cast<uint32_t>(offsetof(FoliageVertex, pos))},
        {1, 0, rhi::Format::RGB32F, static_cast<uint32_t>(offsetof(FoliageVertex, normal))},
        {2, 0, rhi::Format::RGB32F, static_cast<uint32_t>(offsetof(FoliageVertex, color))},
        {3, 1, rhi::Format::RGBA32F, static_cast<uint32_t>(offsetof(FoliageInstance, posScale))},
        {4, 1, rhi::Format::RGBA32F, static_cast<uint32_t>(offsetof(FoliageInstance, params))}
    };
    fDesc.cullMode = rhi::CullMode::None;
    fDesc.depthTestEnable = true;
    fDesc.depthWriteEnable = true;
    fDesc.depthCompareOp = rhi::CompareOp::LessOrEqual;
    fDesc.hasColor = false;
    fDesc.hasDepth = true;
    fDesc.depthFormat = rhi::Format::D32F;
    fDesc.depthBiasConstant = 1.5f;
    fDesc.depthBiasSlope = 2.0f;
    fDesc.usesUniformBuffer = true;
    fDesc.usesStorageBuffer = false;
    fDesc.pushConstantSize = sizeof(ShadowPushConstants);
    fDesc.pushConstantStages = rhi::ShaderStage::Vertex;
    m_foliageShadowPipeline = device.createGraphicsPipeline(fDesc);
}

void ShadowRenderer::updateCascades(const Camera& camera, const glm::vec3& lightDir) {
    glm::vec3 dir = glm::normalize(lightDir);
    if (dir.y < 0.05f) {
        dir.y = 0.05f;
        dir = glm::normalize(dir);
    }

    const float nearClip = EngineConstants::Camera::NEAR_PLANE;
    const float farClip = EngineConstants::Camera::FAR_PLANE;
    const float splits[5] = {nearClip, 32.0f, 105.0f, 280.0f, 720.0f};
    m_cascadeSplits = glm::vec4(splits[1], splits[2], splits[3], splits[4]);

    glm::mat4 invView = glm::inverse(camera.getViewMatrix());
    glm::mat4 proj = camera.getProjMatrix();
    glm::mat4 invProj = glm::inverse(proj);

    auto getDepthAtViewDist = [&](float dist) -> float {
        glm::vec4 clip = proj * glm::vec4(0.0f, 0.0f, -dist, 1.0f);
        return clip.z / clip.w;
    };

    glm::vec3 up = (std::abs(dir.y) > 0.99f) ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);

    for (uint32_t c = 0; c < CASCADE_COUNT; ++c) {
        float zN = getDepthAtViewDist(std::min(splits[c], farClip * 0.99f));
        float zF = getDepthAtViewDist(std::min(splits[c + 1], farClip * 0.99f));

        std::array<glm::vec3, 8> corners{};
        int idx = 0;
        for (int z = 0; z < 2; ++z) {
            float ndcZ = (z == 0) ? zN : zF;
            for (int y = -1; y <= 1; y += 2) {
                for (int x = -1; x <= 1; x += 2) {
                    glm::vec4 viewP = invProj * glm::vec4(static_cast<float>(x), static_cast<float>(y), ndcZ, 1.0f);
                    viewP /= viewP.w;
                    glm::vec4 worldP = invView * viewP;
                    corners[idx++] = glm::vec3(worldP);
                }
            }
        }

        glm::vec3 center(0.0f);
        for (const auto& p : corners) center += p;
        center /= 8.0f;

        float radius = 0.0f;
        for (const auto& p : corners) {
            radius = std::max(radius, glm::length(p - center));
        }
        radius = std::ceil(radius * 16.0f) / 16.0f;

        float pullBack = radius + 450.0f;
        glm::mat4 lightView = glm::lookAt(center + dir * pullBack, center, up);
        glm::mat4 lightProj = glm::ortho(-radius, radius, -radius, radius, 0.1f, pullBack + radius + 150.0f);
        glm::mat4 shadowVP = lightProj * lightView;

        // Texel snapping to prevent shadow edge swimming on camera motion
        glm::vec4 shadowOrigin = shadowVP * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        shadowOrigin *= (static_cast<float>(TILE_RES) * 0.5f);
        glm::vec4 rounded = glm::round(shadowOrigin);
        glm::vec4 roundOffset = (rounded - shadowOrigin) * (2.0f / static_cast<float>(TILE_RES));
        lightProj[3][0] += roundOffset.x;
        lightProj[3][1] += roundOffset.y;

        m_lightSpaceMatrices[c] = lightProj * lightView;
    }
}

void ShadowRenderer::renderShadows(
    rhi::RHICommandList& cmd,
    const rhi::RHIBuffer& globalUBO,
    ChunkManager& chunkManager,
    FoliageRenderer& foliageRenderer,
    StructureRenderer& structureRenderer,
    uint32_t frameIndex,
    const TerrainConfig& config
) {
    if (!config.shadowsEnabled) return;

    cmd.textureBarrier(*m_shadowAtlas, rhi::TextureLayout::ShaderReadOnly, rhi::TextureLayout::DepthAttachment);

    rhi::RenderPassDesc shadowPass{};
    shadowPass.colorTarget = nullptr;
    shadowPass.depthTarget = m_shadowAtlas.get();
    shadowPass.clearColorEnable = false;
    shadowPass.clearDepthEnable = true;
    shadowPass.clearDepth = 1.0f;

    cmd.beginRenderPass(shadowPass);

    for (uint32_t c = 0; c < CASCADE_COUNT; ++c) {
        uint32_t tileX = (c % 2) * TILE_RES;
        uint32_t tileY = (c / 2) * TILE_RES;
        cmd.setViewport(static_cast<float>(tileX), static_cast<float>(tileY), static_cast<float>(TILE_RES), static_cast<float>(TILE_RES));
        cmd.setScissor(static_cast<int32_t>(tileX), static_cast<int32_t>(tileY), TILE_RES, TILE_RES);

        cmd.bindGraphicsPipeline(*m_terrainShadowPipeline);
        cmd.bindUniformBuffer(0, globalUBO);
        chunkManager.recordShadowCommands(cmd, c, config);

        if (config.showFoliage && !config.wireframe && c < 3) {
            cmd.bindGraphicsPipeline(*m_foliageShadowPipeline);
            cmd.bindUniformBuffer(0, globalUBO);
            foliageRenderer.recordShadowCommands(cmd, frameIndex, c);
        }

        if (config.showStructures && !config.wireframe && c < 3) {
            structureRenderer.recordShadowCommands(cmd, globalUBO, frameIndex, c);
        }
    }

    cmd.endRenderPass();
    cmd.textureBarrier(*m_shadowAtlas, rhi::TextureLayout::DepthAttachment, rhi::TextureLayout::ShaderReadOnly);
}
