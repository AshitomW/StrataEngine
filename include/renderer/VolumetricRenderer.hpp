#pragma once

#include "rhi/RHIDevice.hpp"
#include "terrain/TerrainTypes.hpp"
#include <glm/glm.hpp>
#include <memory>

class VolumetricRenderer {
public:
    VolumetricRenderer(rhi::RHIDevice& device, uint32_t width, uint32_t height);
    ~VolumetricRenderer() = default;

    void resize(uint32_t width, uint32_t height);

    void compute(
        rhi::RHICommandList& cmd,
        const rhi::RHITexture& depthTex,
        const rhi::RHITexture& shadowAtlas,
        const GlobalUBO& ubo,
        const TerrainConfig& config,
        uint32_t frameIndex
    );

    const rhi::RHITexture& getGodRaysTexture() const { return *m_godRaysTex; }
    const rhi::RHISampler& getLinearSampler() const { return *m_linearSampler; }

private:
    struct GodRaysData {
        glm::mat4 invViewProj;
        glm::mat4 lightSpaceMatrices[4];
        glm::vec4 cameraPos;
        glm::vec4 sunDir;
        glm::vec4 sunColor;
        glm::vec4 params0;
        glm::vec4 params1;
    };

    void createImages();

    rhi::RHIDevice& m_device;
    uint32_t m_width = 1;
    uint32_t m_height = 1;

    std::unique_ptr<rhi::RHIPipeline> m_pipeline;
    std::unique_ptr<rhi::RHITexture> m_godRaysTex;
    std::unique_ptr<rhi::RHIBuffer> m_paramsBuffer;
    std::unique_ptr<rhi::RHISampler> m_pointSampler;
    std::unique_ptr<rhi::RHISampler> m_linearSampler;
};
