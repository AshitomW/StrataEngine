#pragma once

#include "rhi/RHIDevice.hpp"
#include "terrain/TerrainTypes.hpp"
#include <memory>

class TerrainChunk {
public:
    TerrainChunk(rhi::RHIDevice& device, int chunkX, int chunkZ);
    ~TerrainChunk() = default;

    TerrainChunk(const TerrainChunk&) = delete;
    TerrainChunk& operator=(const TerrainChunk&) = delete;

    TerrainChunk(TerrainChunk&&) noexcept = default;
    TerrainChunk& operator=(TerrainChunk&&) noexcept = default;

    void setCoord(int chunkX, int chunkZ);
    void updateLOD(const glm::vec3& cameraPos, int lodMode);

    int getChunkX() const { return m_chunkX; }
    int getChunkZ() const { return m_chunkZ; }
    glm::vec2 getWorldPos() const { return m_worldPos; }
    uint32_t getLOD() const { return m_lod; }
    void setLOD(uint32_t lod) { m_lod = lod; }

    bool needsRegeneration() const { return m_needsRegeneration; }
    void setRegenerated() { m_needsRegeneration = false; }
    void markDirty() { m_needsRegeneration = true; }

    const rhi::RHIBuffer& getSSBOBuffer() const { return *m_ssboBuffer; }

    float getDistanceTo(const glm::vec3& pos) const;

private:
    int m_chunkX = 0;
    int m_chunkZ = 0;
    glm::vec2 m_worldPos{0.0f, 0.0f};

    uint32_t m_lod = 0;
    bool m_needsRegeneration = true;

    std::unique_ptr<rhi::RHIBuffer> m_ssboBuffer;
};
