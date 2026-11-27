#pragma once

#include "rhi/RHIDevice.hpp"
#include "core/Frustum.hpp"
#include "terrain/TerrainChunk.hpp"
#include "terrain/ComputeTerrainGenerator.hpp"
#include "terrain/TerrainTypes.hpp"
#include <array>
#include <memory>
#include <vector>

class ChunkManager {
public:
    explicit ChunkManager(rhi::RHIDevice& device, int radius = 3);
    ~ChunkManager() = default;

    ChunkManager(const ChunkManager&) = delete;
    ChunkManager& operator=(const ChunkManager&) = delete;

    void update(rhi::RHIDevice& device, const glm::vec3& cameraPos, const TerrainConfig& config);
    void regenerateAll(rhi::RHIDevice& device, const TerrainConfig& config);
    void setRadius(rhi::RHIDevice& device, int newRadius, const TerrainConfig& config);

    void recordRenderCommands(rhi::RHICommandList& cmd, const Frustum& frustum, const TerrainConfig& config);
    void recordShadowCommands(rhi::RHICommandList& cmd, uint32_t cascadeIndex, const TerrainConfig& config);

    size_t getChunkCount() const { return m_chunks.size(); }
    int getRadius() const { return m_radius; }
    int getCenterChunkX() const { return m_centerChunkX; }
    int getCenterChunkZ() const { return m_centerChunkZ; }

    const std::vector<glm::vec2>& getChunkOrigins() const { return m_chunkOrigins; }

private:
    void createLODIndexBuffers(rhi::RHIDevice& device);
    void rebuildChunks(rhi::RHIDevice& device);
    void refreshChunkOrigins();

    static uint32_t getLODGridRes(uint32_t lod) {
        uint32_t step = 1u << lod;
        return (CHUNK_GRID_RES - 1u) / step + 1u;
    }

    static uint32_t getLODStep(uint32_t lod) {
        return 1u << lod;
    }

private:
    int m_radius = 3;
    int m_centerChunkX = 0;
    int m_centerChunkZ = 0;

    std::unique_ptr<ComputeTerrainGenerator> m_generator;
    std::vector<TerrainChunk> m_chunks;
    std::vector<glm::vec2> m_chunkOrigins;
    std::vector<TerrainChunk*> m_dirtyChunks;

    std::array<std::unique_ptr<rhi::RHIBuffer>, NUM_LOD_LEVELS> m_lodIndexBuffers;
    std::array<uint32_t, NUM_LOD_LEVELS> m_lodIndexCounts{};
};
