#pragma once

#include "rhi/RHIDevice.hpp"
#include "terrain/TerrainChunk.hpp"
#include "terrain/TerrainTypes.hpp"
#include <memory>
#include <vector>

class ComputeTerrainGenerator {
public:
    explicit ComputeTerrainGenerator(rhi::RHIDevice& device);
    ~ComputeTerrainGenerator() = default;

    ComputeTerrainGenerator(const ComputeTerrainGenerator&) = delete;
    ComputeTerrainGenerator& operator=(const ComputeTerrainGenerator&) = delete;

    void generateChunks(rhi::RHIDevice& device, const std::vector<TerrainChunk*>& chunks, const TerrainConfig& config);
    void recordGenerateChunks(rhi::RHICommandList& cmd, const std::vector<TerrainChunk*>& chunks, const TerrainConfig& config);

private:
    std::unique_ptr<rhi::RHIPipeline> m_pipeline;
};
