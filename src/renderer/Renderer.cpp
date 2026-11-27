#include "renderer/Renderer.hpp"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <random>

Renderer::Renderer(rhi::RHIDevice& device, GLFWwindow* window)
    : m_device(device) {
    createUBOBuffers();
    createPipelines();

    m_foliageRenderer = std::make_unique<FoliageRenderer>(device);
    m_skyRenderer = std::make_unique<SkyRenderer>(device);
    m_waterRenderer = std::make_unique<WaterRenderer>(device);

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
    solidDesc.usesUniformBuffer = true;
    solidDesc.usesStorageBuffer = true;
    solidDesc.pushConstantSize = sizeof(ChunkPushConstants);
    solidDesc.pushConstantStages = rhi::ShaderStage::Vertex;

    m_solidPipeline = m_device.createGraphicsPipeline(solidDesc);

    rhi::GraphicsPipelineDesc wireDesc = solidDesc;
    wireDesc.polygonMode = rhi::PolygonMode::Line;
    m_wireframePipeline = m_device.createGraphicsPipeline(wireDesc);
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

    m_uboBuffers[frameIndex]->copyFromHost(&ubo, sizeof(GlobalUBO));
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
    ImGui::SetNextWindowSize(ImVec2(390.0f, 540.0f), ImGuiCond_FirstUseEver);
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

        if (ImGui::CollapsingHeader("Atmosphere & Time", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("Time of Day [[ / ]]", &config.timeOfDay, 0.0f, 24.0f, "%.2f h");
            ImGui::Checkbox("Run Day/Night Cycle [T]", &config.timeCycleRunning);
            ImGui::SliderFloat("Cycle Speed", &config.timeCycleSpeed, 0.0f, 2.0f, "%.2f x");
        }

        if (ImGui::CollapsingHeader("Water & Foliage", ImGuiTreeNodeFlags_DefaultOpen)) {
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
    rhi::ClearColor clearColor{sky.skyClearColor.r, sky.skyClearColor.g, sky.skyClearColor.b, sky.skyClearColor.a};

    cmd->beginRenderPass(clearColor, 1.0f);

    float w = static_cast<float>(m_device.getSwapchainWidth());
    float h = static_cast<float>(m_device.getSwapchainHeight());
    cmd->setViewport(0.0f, 0.0f, w, h);
    cmd->setScissor(0, 0, m_device.getSwapchainWidth(), m_device.getSwapchainHeight());

    FrameContext frame;
    frame.cmd = cmd;
    frame.frameIndex = frameIndex;
    frame.globalUBO = m_uboBuffers[frameIndex].get();
    frame.camera = &camera;
    frame.config = &config;
    frame.time = time;
    frame.screenSize = glm::vec2(w, h);

    m_skyRenderer->recordRenderCommands(frame);

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

    m_device.renderImGui(*cmd);
    cmd->endRenderPass();

    m_device.endFrame(window);
    return true;
}
