#include "terrain/ComputeTerrainGenerator.hpp"

ComputeTerrainGenerator::ComputeTerrainGenerator(rhi::RHIDevice& device) {
    rhi::ComputePipelineDesc desc{};
    desc.computeShader = "terrain.comp";
    desc.numStorageBuffers = 1;
    desc.numUniformBuffers = 0;
    desc.pushConstantSize = sizeof(ComputePushConstants);
    desc.localSizeX = 16;
    desc.localSizeY = 16;
    desc.localSizeZ = 1;

    m_pipeline = device.createComputePipeline(desc);
}

void ComputeTerrainGenerator::generateChunks(rhi::RHIDevice& device, const std::vector<TerrainChunk*>& chunks, const TerrainConfig& config) {
    if (chunks.empty()) return;

    device.executeImmediateCompute([&](rhi::RHICommandList& cmd) {
        cmd.bindComputePipeline(*m_pipeline);

        for (TerrainChunk* chunk : chunks) {
            cmd.bindStorageBuffer(0, chunk->getSSBOBuffer());

            ComputePushConstants pc{};
            pc.chunkOffset = glm::vec4(chunk->getWorldPos().x, chunk->getWorldPos().y, CHUNK_CELL_SIZE, static_cast<float>(CHUNK_GRID_RES));
            pc.noiseParams1 = glm::vec4(config.frequency, config.amplitude, config.warpStrength, config.mountainPower);
            pc.noiseParams2 = glm::vec4(config.octaves, config.lacunarity, config.persistence, config.waterHeight);
            pc.config = glm::uvec4(config.seed, CHUNK_GRID_RES, config.presetType, 0);

            cmd.pushConstants(rhi::ShaderStage::Compute, &pc, sizeof(ComputePushConstants));

            uint32_t groupCount = (CHUNK_GRID_RES + 15) / 16;
            cmd.dispatch(groupCount, groupCount, 1);
            cmd.bufferBarrier(chunk->getSSBOBuffer());

            chunk->setRegenerated();
        }
    });
}
