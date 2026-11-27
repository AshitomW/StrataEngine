#include "terrain/TerrainChunk.hpp"
#include <cmath>

TerrainChunk::TerrainChunk(rhi::RHIDevice& device, int chunkX, int chunkZ)
    : m_chunkX(chunkX), m_chunkZ(chunkZ) {
    m_worldPos = glm::vec2(static_cast<float>(chunkX) * CHUNK_SIZE, static_cast<float>(chunkZ) * CHUNK_SIZE);
    size_t bufferSize = static_cast<size_t>(CHUNK_GRID_RES) * CHUNK_GRID_RES * sizeof(glm::vec4);
    m_ssboBuffer = device.createBuffer(bufferSize, rhi::BufferUsage::Storage, rhi::MemoryUsage::GpuOnly);
}

void TerrainChunk::setCoord(int chunkX, int chunkZ) {
    if (m_chunkX != chunkX || m_chunkZ != chunkZ) {
        m_chunkX = chunkX;
        m_chunkZ = chunkZ;
        m_worldPos = glm::vec2(static_cast<float>(chunkX) * CHUNK_SIZE, static_cast<float>(chunkZ) * CHUNK_SIZE);
        m_needsRegeneration = true;
    }
}

float TerrainChunk::getDistanceTo(const glm::vec3& pos) const {
    glm::vec2 chunkCenter = m_worldPos + glm::vec2(CHUNK_SIZE * 0.5f, CHUNK_SIZE * 0.5f);
    float dx = chunkCenter.x - pos.x;
    float dz = chunkCenter.y - pos.z;
    return std::sqrt(dx * dx + dz * dz);
}

void TerrainChunk::updateLOD(const glm::vec3& cameraPos, int lodMode) {
    if (lodMode >= 0 && lodMode < static_cast<int>(NUM_LOD_LEVELS)) {
        m_lod = static_cast<uint32_t>(lodMode);
        return;
    }

    float dist = getDistanceTo(cameraPos);

    if (dist < 180.0f) {
        m_lod = 0;
    } else if (dist < 360.0f) {
        m_lod = 1;
    } else if (dist < 620.0f) {
        m_lod = 2;
    } else {
        m_lod = 3;
    }
}
