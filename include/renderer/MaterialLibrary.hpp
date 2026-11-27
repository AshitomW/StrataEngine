#pragma once

#include "rhi/RHIDevice.hpp"
#include <memory>

class MaterialLibrary {
public:
    static constexpr uint32_t TEXTURE_RES = 1024;
    static constexpr uint32_t LAYER_COUNT = 8;

    explicit MaterialLibrary(rhi::RHIDevice& device);
    ~MaterialLibrary() = default;

    MaterialLibrary(const MaterialLibrary&) = delete;
    MaterialLibrary& operator=(const MaterialLibrary&) = delete;

    const rhi::RHITexture& getAlbedoHeightArray() const { return *m_albedoHeightArray; }
    const rhi::RHITexture& getNormalORMArray() const { return *m_normalORMArray; }
    const rhi::RHISampler& getMaterialSampler() const { return *m_materialSampler; }

private:
    std::unique_ptr<rhi::RHITexture> m_albedoHeightArray;
    std::unique_ptr<rhi::RHITexture> m_normalORMArray;
    std::unique_ptr<rhi::RHISampler> m_materialSampler;
    std::unique_ptr<rhi::RHIPipeline> m_bakePipeline;
};

