#pragma once

#include "rhi/RHIDevice.hpp"
#include "camera/Camera.hpp"
#include "terrain/TerrainTypes.hpp"

class ShadowRenderer;

struct FrameContext {
    rhi::RHICommandList* cmd = nullptr;
    uint32_t frameIndex = 0;
    const rhi::RHIBuffer* globalUBO = nullptr;
    const ShadowRenderer* shadows = nullptr;
    const Camera* camera = nullptr;
    const TerrainConfig* config = nullptr;
    float time = 0.0f;
    glm::vec2 screenSize{0.0f, 0.0f};
};
