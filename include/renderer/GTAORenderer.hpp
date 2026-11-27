#pragma once

#include "rhi/RHIDevice.hpp"
#include "camera/Camera.hpp"
#include "terrain/TerrainTypes.hpp"
#include <memory>

class GTAORenderer {
public:
    explicit GTAORenderer(rhi::RHIDevice& device);
    ~GTAORenderer() = default;

    GTAORenderer(const GTAORenderer&) = delete;
    GTAORenderer& operator=(const GTAORenderer&) = delete;

    void resize(rhi::RHIDevice& device, uint32_t fullWidth, uint32_t fullHeight);

    void compute(
        rhi::RHICommandList& cmd,
        const rhi::RHITexture& sceneDepth,
        const Camera& camera,
        uint32_t frameIndex,
        const TerrainConfig& config
    );

    const rhi::RHITexture& getAOTexture() const { return *m_aoBlur; }
    const rhi::RHISampler& getSampler() const { return *m_sampler; }

private:
    struct GTAOPushConstants {
        glm::mat4 invProj;
        glm::vec4 params;
    };

    uint32_t m_halfWidth = 0;
    uint32_t m_halfHeight = 0;

    std::unique_ptr<rhi::RHITexture> m_aoRaw;
    std::unique_ptr<rhi::RHITexture> m_aoBlur;
    std::unique_ptr<rhi::RHISampler> m_sampler;
    std::unique_ptr<rhi::RHIPipeline> m_gtaoPipeline;
    std::unique_ptr<rhi::RHIPipeline> m_blurPipeline;
};

