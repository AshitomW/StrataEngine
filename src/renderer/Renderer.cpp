#include "renderer/Renderer.hpp"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <random>

namespace {

struct BloomDownPC {
    glm::vec4 params;
};

struct BloomUpPC {
    glm::vec4 params;
};

} // namespace

Renderer::Renderer(rhi::RHIDevice& device, GLFWwindow* window)
    : m_device(device) {
    rhi::SamplerDesc sampDesc{};
    sampDesc.minFilter = rhi::Filter::Linear;
    sampDesc.magFilter = rhi::Filter::Linear;
    sampDesc.addressU = rhi::AddressMode::ClampToEdge;
    sampDesc.addressV = rhi::AddressMode::ClampToEdge;
    sampDesc.addressW = rhi::AddressMode::ClampToEdge;
    m_linearSampler = device.createSampler(sampDesc);

    createUBOBuffers();
    createPipelines();

    m_atmosphereRenderer = std::make_unique<AtmosphereRenderer>(device);
    m_foliageRenderer = std::make_unique<FoliageRenderer>(device);
    m_skyRenderer = std::make_unique<SkyRenderer>(device);
    m_waterRenderer = std::make_unique<WaterRenderer>(device);

    rhi::TextureDesc blackDesc{};
    blackDesc.width = 1;
    blackDesc.height = 1;
    blackDesc.format = rhi::Format::RGBA16F;
    blackDesc.usage = rhi::TextureUsage::Sampled | rhi::TextureUsage::Storage;
    m_blackFallbackTex = device.createTexture(blackDesc);
    device.executeImmediateCompute([&](rhi::RHICommandList& cmd) {
        cmd.textureBarrier(*m_blackFallbackTex, rhi::TextureLayout::Undefined, rhi::TextureLayout::ShaderReadOnly);
    });

    createScreenResources(device.getSwapchainWidth(), device.getSwapchainHeight());
    m_device.initImGui(window);
}

Renderer::~Renderer() {
    m_device.waitIdle();
    m_device.shutdownImGui();
}

void Renderer::createUBOBuffers() {
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        m_uboBuffers[i] = m_device.createBuffer(
            sizeof(GlobalUBO),
            rhi::BufferUsage::Uniform,
            rhi::MemoryUsage::CpuToGpu
        );
    }
}

void Renderer::createPipelines() {
    rhi::GraphicsPipelineDesc solidDesc{};
    solidDesc.vertexShader = "terrain.vert";
    solidDesc.fragmentShader = "terrain.frag";
    solidDesc.cullMode = rhi::CullMode::None;
    solidDesc.polygonMode = rhi::PolygonMode::Fill;
    solidDesc.depthTestEnable = true;
    solidDesc.depthWriteEnable = true;
    solidDesc.depthCompareOp = rhi::CompareOp::LessOrEqual;
    solidDesc.blendEnable = false;
    solidDesc.colorFormat = rhi::Format::RGBA16F;
    solidDesc.depthFormat = rhi::Format::D32F;
    solidDesc.hasDepth = true;
    solidDesc.usesUniformBuffer = true;
    solidDesc.usesStorageBuffer = true;
    solidDesc.pushConstantSize = sizeof(ChunkPushConstants);
    solidDesc.pushConstantStages = rhi::ShaderStage::Vertex;

    m_solidPipeline = m_device.createGraphicsPipeline(solidDesc);

    rhi::GraphicsPipelineDesc wireDesc = solidDesc;
    wireDesc.polygonMode = rhi::PolygonMode::Line;
    m_wireframePipeline = m_device.createGraphicsPipeline(wireDesc);

    rhi::ComputePipelineDesc bdDesc{};
    bdDesc.computeShader = "bloom_downsample.comp";
    bdDesc.numStorageBuffers = 0;
    bdDesc.numSampledTextures = 1;
    bdDesc.numStorageTextures = 1;
    bdDesc.pushConstantSize = sizeof(BloomDownPC);
    bdDesc.localSizeX = 8;
    bdDesc.localSizeY = 8;
    m_bloomDownPipeline = m_device.createComputePipeline(bdDesc);

    rhi::ComputePipelineDesc buDesc{};
    buDesc.computeShader = "bloom_upsample.comp";
    buDesc.numStorageBuffers = 0;
    buDesc.numSampledTextures = 2;
    buDesc.numStorageTextures = 1;
    buDesc.pushConstantSize = sizeof(BloomUpPC);
    buDesc.localSizeX = 8;
    buDesc.localSizeY = 8;
    m_bloomUpPipeline = m_device.createComputePipeline(buDesc);

    rhi::GraphicsPipelineDesc ppDesc{};
    ppDesc.vertexShader = "postprocess.vert";
    ppDesc.fragmentShader = "postprocess.frag";
    ppDesc.cullMode = rhi::CullMode::None;
    ppDesc.depthTestEnable = false;
    ppDesc.depthWriteEnable = false;
    ppDesc.depthCompareOp = rhi::CompareOp::Always;
    ppDesc.blendEnable = false;
    ppDesc.colorFormat = rhi::Format::BGRA8_SRGB;
    ppDesc.hasDepth = false;
    ppDesc.usesUniformBuffer = true;
    ppDesc.usesStorageBuffer = false;
    ppDesc.numSampledTextures = 2;
    m_postProcessPipeline = m_device.createGraphicsPipeline(ppDesc);
}

void Renderer::createScreenResources(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) return;
    if (width == m_screenWidth && height == m_screenHeight) return;

    m_device.waitIdle();
    m_screenWidth = width;
    m_screenHeight = height;

    rhi::TextureDesc hdrDesc{};
    hdrDesc.width = width;
    hdrDesc.height = height;
    hdrDesc.format = rhi::Format::RGBA16F;
    hdrDesc.usage = rhi::TextureUsage::ColorAttachment | rhi::TextureUsage::Sampled;
    m_hdrColor = m_device.createTexture(hdrDesc);

    rhi::TextureDesc depthDesc{};
    depthDesc.width = width;
    depthDesc.height = height;
    depthDesc.format = rhi::Format::D32F;
    depthDesc.usage = rhi::TextureUsage::DepthAttachment | rhi::TextureUsage::Sampled;
    m_sceneDepth = m_device.createTexture(depthDesc);

    uint32_t mw = std::max(1u, width / 2);
    uint32_t mh = std::max(1u, height / 2);
    for (uint32_t i = 0; i < BLOOM_MIP_COUNT; ++i) {
        rhi::TextureDesc mipDesc{};
        mipDesc.width = mw;
        mipDesc.height = mh;
        mipDesc.format = rhi::Format::RGBA16F;
        mipDesc.usage = rhi::TextureUsage::Sampled | rhi::TextureUsage::Storage;
        m_bloomDownMips[i] = m_device.createTexture(mipDesc);
        if (i < BLOOM_MIP_COUNT - 1) {
            m_bloomUpMips[i] = m_device.createTexture(mipDesc);
        }
        mw = std::max(1u, mw / 2);
        mh = std::max(1u, mh / 2);
    }

    m_device.executeImmediateCompute([&](rhi::RHICommandList& cmd) {
        cmd.textureBarrier(*m_hdrColor, rhi::TextureLayout::Undefined, rhi::TextureLayout::ShaderReadOnly);
        for (uint32_t i = 0; i < BLOOM_MIP_COUNT; ++i) {
            cmd.textureBarrier(*m_bloomDownMips[i], rhi::TextureLayout::Undefined, rhi::TextureLayout::ShaderReadOnly);
            if (i < BLOOM_MIP_COUNT - 1) {
                cmd.textureBarrier(*m_bloomUpMips[i], rhi::TextureLayout::Undefined, rhi::TextureLayout::ShaderReadOnly);
            }
        }
    });
}

float Renderer::getAspectRatio() const {
    return m_device.getAspectRatio();
}

void Renderer::updateUBO(uint32_t frameIndex, const Camera& camera, const TerrainConfig& config, float time) {
    GlobalUBO ubo{};
    ubo.viewProj = camera.getViewProjMatrix();
    ubo.view = camera.getViewMatrix();
    ubo.proj = camera.getProjMatrix();
    ubo.cameraPos = glm::vec4(camera.getPosition(), 1.0f);

    Atmosphere::State sky = Atmosphere::compute(config.timeOfDay);

    ubo.sunDir = glm::vec4(sky.activeLightDir, sky.sinElevation);
    ubo.sunColor = glm::vec4(sky.sunColor, sky.dayFactor);
    ubo.skyColorZenith = glm::vec4(sky.skyZenith, sky.ambientIntensity);
    ubo.skyColorHorizon = glm::vec4(sky.skyHorizon, sky.starFactor);

    float maxFogDist = static_cast<float>(config.viewRadius) * CHUNK_SIZE * 0.95f;
    ubo.terrainParams = glm::vec4(config.waterHeight, config.fogDensity, time, config.debugMode);
    ubo.biomeParams = glm::vec4(config.amplitude, static_cast<float>(config.presetType), static_cast<float>(config.seed), maxFogDist);
    ubo.postParams = glm::vec4(
        config.exposure,
        config.bloomEnabled ? config.bloomIntensity : 0.0f,
        config.bloomThreshold,
        static_cast<float>(config.toneMapper)
    );
    ubo.atmosphereParams = glm::vec4(config.turbidity, config.mieAnisotropy, config.sunIntensity, config.iblStrength);

    m_uboBuffers[frameIndex]->copyFromHost(&ubo, sizeof(GlobalUBO));
}

void Renderer::runBloomPasses(rhi::RHICommandList& cmd, const TerrainConfig& config) {
    if (!config.bloomEnabled || config.bloomIntensity <= 0.001f) {
        return;
    }

    // Downsample chain
    cmd.bindComputePipeline(*m_bloomDownPipeline);
    for (uint32_t i = 0; i < BLOOM_MIP_COUNT; ++i) {
        const rhi::RHITexture& srcTex = (i == 0) ? *m_hdrColor : *m_bloomDownMips[i - 1];
        rhi::RHITexture& dstTex = *m_bloomDownMips[i];

        cmd.textureBarrier(dstTex, rhi::TextureLayout::ShaderReadOnly, rhi::TextureLayout::General);
        cmd.bindTexture(0, srcTex, *m_linearSampler);
        cmd.bindStorageTexture(1, dstTex);

        BloomDownPC pc{};
        pc.params = glm::vec4(config.bloomThreshold, config.bloomThreshold * 0.5f, (i == 0) ? 1.0f : 0.0f, 0.0f);
        cmd.pushConstants(rhi::ShaderStage::Compute, &pc, sizeof(BloomDownPC));

        cmd.dispatch((dstTex.getWidth() + 7) / 8, (dstTex.getHeight() + 7) / 8, 1);
        cmd.textureBarrier(dstTex, rhi::TextureLayout::General, rhi::TextureLayout::ShaderReadOnly);
    }

    // Upsample tent-filter chain
    cmd.bindComputePipeline(*m_bloomUpPipeline);
    for (int i = static_cast<int>(BLOOM_MIP_COUNT) - 2; i >= 0; --i) {
        const rhi::RHITexture& baseTex = *m_bloomDownMips[i];
        const rhi::RHITexture& smallTex = (i == static_cast<int>(BLOOM_MIP_COUNT) - 2)
            ? *m_bloomDownMips[i + 1]
            : *m_bloomUpMips[i + 1];
        rhi::RHITexture& outTex = *m_bloomUpMips[i];

        cmd.textureBarrier(outTex, rhi::TextureLayout::ShaderReadOnly, rhi::TextureLayout::General);
        cmd.bindTexture(0, baseTex, *m_linearSampler);
        cmd.bindTexture(1, smallTex, *m_linearSampler);
        cmd.bindStorageTexture(2, outTex);

        BloomUpPC pc{};
        pc.params = glm::vec4(1.0f, 0.65f, 1.0f, 0.0f);
        cmd.pushConstants(rhi::ShaderStage::Compute, &pc, sizeof(BloomUpPC));

        cmd.dispatch((outTex.getWidth() + 7) / 8, (outTex.getHeight() + 7) / 8, 1);
        cmd.textureBarrier(outTex, rhi::TextureLayout::General, rhi::TextureLayout::ShaderReadOnly);
    }
}

void Renderer::drawImGui(const Camera& camera, ChunkManager& chunkManager, TerrainConfig& config, HUDInfo& hudInfo) {
    if (!hudInfo.showHUD) {
        ImGui::SetNextWindowPos(ImVec2(12.0f, 12.0f), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.75f);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                 ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs;
        if (ImGui::Begin("##MinimalHUD", nullptr, flags)) {
            ImGui::Text("[H] Show HUD | [Tab] Unlock Mouse | %s | %.0f FPS (%.2f ms GPU)",
                        m_device.getBackendName(), hudInfo.fps, m_device.getLastGpuFrameTimeMs());
        }
        ImGui::End();
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(14.0f, 14.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400.0f, 620.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.86f);

    if (ImGui::Begin("Terrain Engine Control Panel", &hudInfo.showHUD)) {
        if (ImGui::CollapsingHeader("Engine & Stats", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Backend: %s", m_device.getBackendName());
            ImGui::Text("GPU: %s", m_device.getDeviceName().c_str());
            ImGui::Text("Hardware RT: %s", m_device.supportsHardwareRayTracing() ? "Supported" : "Software Fallback");
            ImGui::Text("FPS: %.1f | GPU: %.2f ms | Chunks: %zu",
                        hudInfo.fps, m_device.getLastGpuFrameTimeMs(), hudInfo.activeChunks);
            glm::vec3 pos = camera.getPosition();
            ImGui::Text("Camera: (%.1f, %.1f, %.1f)", pos.x, pos.y, pos.z);
            ImGui::Text("Mouse: %s ([Tab] to toggle)", hudInfo.mouseCaptured ? "CAPTURED" : "UNLOCKED");
        }

        bool needsRegen = false;

        if (ImGui::CollapsingHeader("Terrain & LOD", ImGuiTreeNodeFlags_DefaultOpen)) {
            const char* presetItems[] = {"Mountains", "Rolling Hills", "Deep Canyons", "Tropical Islands", "Multi-Biome World"};
            int presetIdx = static_cast<int>(std::min(config.presetType, 4u));
            if (ImGui::Combo("Preset [1-5]", &presetIdx, presetItems, IM_ARRAYSIZE(presetItems))) {
                switch (presetIdx) {
                    case 0: config.applyMountains(); hudInfo.presetName = "Mountains"; break;
                    case 1: config.applyHills(); hudInfo.presetName = "Rolling Hills"; break;
                    case 2: config.applyCanyons(); hudInfo.presetName = "Deep Canyons"; break;
                    case 3: config.applyIslands(); hudInfo.presetName = "Tropical Islands"; break;
                    case 4: config.applyMultiBiome(); hudInfo.presetName = "Multi-Biome World"; break;
                }
                needsRegen = true;
            }

            ImGui::Text("Seed: %u", config.seed);
            ImGui::SameLine();
            if (ImGui::Button("Randomize [R]")) {
                static std::mt19937 gen(std::random_device{}());
                config.seed = std::uniform_int_distribution<uint32_t>(1, 999999)(gen);
                hudInfo.presetName = "Custom (Random)";
                needsRegen = true;
            }

            if (ImGui::SliderFloat("Amplitude [Z/X]", &config.amplitude, 10.0f, 220.0f, "%.1f m")) {
                hudInfo.presetName = "Custom";
                needsRegen = true;
            }
            if (ImGui::SliderFloat("Frequency [C/V]", &config.frequency, 0.2f, 3.0f, "%.2f")) {
                hudInfo.presetName = "Custom";
                needsRegen = true;
            }

            int viewRadius = config.viewRadius;
            if (ImGui::SliderInt("View Radius [-/=]", &viewRadius, EngineConstants::Terrain::MIN_VIEW_RADIUS, EngineConstants::Terrain::MAX_VIEW_RADIUS)) {
                config.viewRadius = viewRadius;
                config.updateFog();
                chunkManager.setRadius(m_device, config.viewRadius, config);
            }

            const char* lodItems[] = {"Auto Distance (0..3)", "Forced LOD 0 (65x65)", "Forced LOD 1 (33x33)", "Forced LOD 2 (17x17)", "Forced LOD 3 (9x9)"};
            int lodCombo = config.lodMode + 1;
            if (ImGui::Combo("LOD Mode [L]", &lodCombo, lodItems, IM_ARRAYSIZE(lodItems))) {
                config.lodMode = lodCombo - 1;
            }

            const char* debugItems[] = {"Realistic Biomes", "LOD Level Colors", "Surface Normals", "Slope Steepness"};
            int debugIdx = static_cast<int>(config.debugMode);
            if (ImGui::Combo("Shading Mode [M]", &debugIdx, debugItems, IM_ARRAYSIZE(debugItems))) {
                config.debugMode = static_cast<float>(debugIdx);
            }

            ImGui::Checkbox("Wireframe [F]", &config.wireframe);
        }

        if (ImGui::CollapsingHeader("Lighting, Atmosphere & Post-FX", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("Time of Day [[ / ]]", &config.timeOfDay, 0.0f, 24.0f, "%.2f h");
            ImGui::Checkbox("Run Day/Night Cycle [T]", &config.timeCycleRunning);
            ImGui::SliderFloat("Cycle Speed", &config.timeCycleSpeed, 0.0f, 2.0f, "%.2f x");

            ImGui::Separator();
            const char* tmItems[] = {"AgX (Punchy)", "ACES Filmic", "Reinhard Extended"};
            ImGui::Combo("Tone Mapper", &config.toneMapper, tmItems, IM_ARRAYSIZE(tmItems));
            ImGui::SliderFloat("Exposure", &config.exposure, 0.2f, 3.0f, "%.2f");
            ImGui::Checkbox("HDR Bloom", &config.bloomEnabled);
            if (config.bloomEnabled) {
                ImGui::SliderFloat("Bloom Intensity", &config.bloomIntensity, 0.0f, 1.5f, "%.2f");
                ImGui::SliderFloat("Bloom Threshold", &config.bloomThreshold, 0.5f, 3.0f, "%.2f");
            }

            ImGui::Separator();
            ImGui::SliderFloat("Sun Intensity", &config.sunIntensity, 0.5f, 6.0f, "%.2f");
            ImGui::SliderFloat("Sky IBL Strength", &config.iblStrength, 0.2f, 2.5f, "%.2f");
            ImGui::SliderFloat("Atmo Turbidity", &config.turbidity, 0.3f, 4.0f, "%.2f");
            ImGui::SliderFloat("Mie Anisotropy", &config.mieAnisotropy, 0.2f, 0.92f, "%.2f");
        }

        if (ImGui::CollapsingHeader("Water & Foliage")) {
            ImGui::Checkbox("Show Water [G]", &config.showWater);
            if (ImGui::SliderFloat("Water Elevation [O/P]", &config.waterHeight, -40.0f, 60.0f, "%.1f m")) {
                needsRegen = true;
            }
            ImGui::SliderFloat("Wave Amplitude [B/N]", &config.waveAmplitude, 0.0f, 2.5f, "%.2f m");
            ImGui::SliderFloat("Wave Speed", &config.waveSpeed, 0.0f, 3.0f, "%.2f x");

            ImGui::Separator();
            ImGui::Checkbox("Show Foliage [E]", &config.showFoliage);
            ImGui::SliderFloat("Foliage Density [U/I]", &config.foliageDensity, 0.0f, EngineConstants::Environment::MAX_FOLIAGE_DENSITY, "%.2f");
        }

        if (needsRegen) {
            chunkManager.regenerateAll(m_device, config);
        }
    }
    ImGui::End();
}

bool Renderer::renderFrame(
    GLFWwindow* window,
    const Camera& camera,
    ChunkManager& chunkManager,
    TerrainConfig& config,
    HUDInfo& hudInfo,
    float time
) {
    uint32_t frameIndex = 0;
    rhi::RHICommandList* cmd = m_device.beginFrame(window, frameIndex);
    if (!cmd) {
        return false;
    }

    uint32_t swW = m_device.getSwapchainWidth();
    uint32_t swH = m_device.getSwapchainHeight();
    if (swW != m_screenWidth || swH != m_screenHeight) {
        createScreenResources(swW, swH);
    }

    updateUBO(frameIndex, camera, config, time);

    FoliageKey key;
    key.seed = config.seed;
    key.centerChunkX = chunkManager.getCenterChunkX();
    key.centerChunkZ = chunkManager.getCenterChunkZ();
    key.radius = chunkManager.getRadius();
    key.showFoliage = config.showFoliage;
    key.presetType = config.presetType;
    key.waterHeight = config.waterHeight;
    key.foliageDensity = config.foliageDensity;

    if (!(key == m_lastFoliageKey)) {
        m_lastFoliageKey = key;
        m_foliageRenderer->updateInstances(m_device, chunkManager.getChunkOrigins(), config);
    }

    m_device.beginImGuiFrame();
    drawImGui(camera, chunkManager, config, hudInfo);

    Atmosphere::State sky = Atmosphere::compute(config.timeOfDay);

    // 1. Update Physical Sky-View LUT (Hillaire 2020)
    m_atmosphereRenderer->updateSkyView(
        *cmd,
        sky.sunDir,
        sky.sunColor,
        config.turbidity,
        config.mieAnisotropy,
        config.sunIntensity
    );

    // 2. Offscreen Linear HDR Render Pass
    rhi::RenderPassDesc hdrPass{};
    hdrPass.colorTarget = m_hdrColor.get();
    hdrPass.depthTarget = m_sceneDepth.get();
    hdrPass.clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
    hdrPass.clearDepth = 1.0f;
    hdrPass.clearColorEnable = true;
    hdrPass.clearDepthEnable = true;

    cmd->beginRenderPass(hdrPass);

    float w = static_cast<float>(swW);
    float h = static_cast<float>(swH);
    cmd->setViewport(0.0f, 0.0f, w, h);
    cmd->setScissor(0, 0, swW, swH);

    FrameContext frame;
    frame.cmd = cmd;
    frame.frameIndex = frameIndex;
    frame.globalUBO = m_uboBuffers[frameIndex].get();
    frame.camera = &camera;
    frame.config = &config;
    frame.time = time;
    frame.screenSize = glm::vec2(w, h);

    m_skyRenderer->recordRenderCommands(frame, *m_atmosphereRenderer);

    const rhi::RHIPipeline& activePipeline = (config.wireframe && m_wireframePipeline && m_wireframePipeline->isValid())
        ? *m_wireframePipeline
        : *m_solidPipeline;
    cmd->bindGraphicsPipeline(activePipeline);
    cmd->bindUniformBuffer(0, *m_uboBuffers[frameIndex]);

    Frustum frustum = Frustum::fromViewProj(camera.getViewProjMatrix());
    chunkManager.recordRenderCommands(*cmd, frustum, config);

    if (config.showFoliage && !config.wireframe) {
        m_foliageRenderer->recordRenderCommands(frame);
    }

    if (config.showWater && !config.wireframe) {
        m_waterRenderer->recordRenderCommands(frame);
    }

    cmd->endRenderPass();

    // 3. Transition HDR target & run Bloom pyramid
    cmd->textureBarrier(*m_hdrColor, rhi::TextureLayout::ColorAttachment, rhi::TextureLayout::ShaderReadOnly);
    runBloomPasses(*cmd, config);

    // 4. Final Swapchain Pass: AgX/ACES Tone Mapping + Bloom Composite + ImGui
    rhi::RenderPassDesc swapPass{};
    swapPass.colorTarget = nullptr;
    swapPass.depthTarget = nullptr;
    swapPass.clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
    swapPass.clearDepth = 1.0f;
    cmd->beginRenderPass(swapPass);

    cmd->setViewport(0.0f, 0.0f, w, h);
    cmd->setScissor(0, 0, swW, swH);

    cmd->bindGraphicsPipeline(*m_postProcessPipeline);
    cmd->bindUniformBuffer(0, *m_uboBuffers[frameIndex]);
    cmd->bindTexture(0, *m_hdrColor, *m_linearSampler);
    const rhi::RHITexture& bloomSrc = (config.bloomEnabled && config.bloomIntensity > 0.001f)
        ? *m_bloomUpMips[0]
        : *m_blackFallbackTex;
    cmd->bindTexture(1, bloomSrc, *m_linearSampler);
    cmd->draw(3, 1, 0);

    m_device.renderImGui(*cmd);
    cmd->endRenderPass();

    m_device.endFrame(window);
    return true;
}
