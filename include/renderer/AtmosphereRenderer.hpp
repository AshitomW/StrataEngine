#pragma once

#include "rhi/RHIDevice.hpp"
#include <glm/glm.hpp>
#include <memory>

class AtmosphereRenderer {
public:
    explicit AtmosphereRenderer(rhi::RHIDevice& device);
    ~AtmosphereRenderer() = default;

    AtmosphereRenderer(const AtmosphereRenderer&) = delete;
    AtmosphereRenderer& operator=(const AtmosphereRenderer&) = delete;

    void updateSkyView(
        rhi::RHICommandList& cmd,
        const glm::vec3& trueSunDir,
        const glm::vec3& sunColor,
        float turbidity,
        float mieAnisotropy,
        float sunIntensity
    );

    const rhi::RHITexture& getTransmittanceLUT() const { return *m_transmittanceLUT; }
    const rhi::RHITexture& getMultiScatterLUT() const { return *m_multiScatterLUT; }
    const rhi::RHITexture& getSkyViewLUT() const { return *m_skyViewLUT; }
    const rhi::RHISampler& getLinearSampler() const { return *m_linearSampler; }

private:
    struct AtmoPushConstants {
        glm::vec4 sunDirTurbidity;
        glm::vec4 sunColorAnisotropy;
    };

    std::unique_ptr<rhi::RHITexture> m_transmittanceLUT;
    std::unique_ptr<rhi::RHITexture> m_multiScatterLUT;
    std::unique_ptr<rhi::RHITexture> m_skyViewLUT;
    std::unique_ptr<rhi::RHISampler> m_linearSampler;

    std::unique_ptr<rhi::RHIPipeline> m_transmittancePipeline;
    std::unique_ptr<rhi::RHIPipeline> m_multiScatterPipeline;
    std::unique_ptr<rhi::RHIPipeline> m_skyViewPipeline;
};
