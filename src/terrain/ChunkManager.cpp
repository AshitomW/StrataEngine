#include "terrain/ChunkManager.hpp"
#include <cmath>

ChunkManager::ChunkManager(rhi::RHIDevice& device, int radius)
    : m_radius(radius) {
    createLODIndexBuffers(device);
    m_generator = std::make_unique<ComputeTerrainGenerator>(device);
    rebuildChunks(device);
}

void ChunkManager::createLODIndexBuffers(rhi::RHIDevice& device) {
    for (uint32_t lod = 0; lod < NUM_LOD_LEVELS; ++lod) {
        uint32_t gridRes = getLODGridRes(lod);
        uint32_t quads = gridRes - 1;

        std::vector<uint32_t> indices;
        indices.reserve(quads * quads * 6);

        for (uint32_t z = 0; z < quads; ++z) {
            for (uint32_t x = 0; x < quads; ++x) {
                uint32_t i0 = z * gridRes + x;
                uint32_t i1 = z * gridRes + (x + 1);
                uint32_t i2 = (z + 1) * gridRes + x;
                uint32_t i3 = (z + 1) * gridRes + (x + 1);

                indices.push_back(i0);
                indices.push_back(i2);
                indices.push_back(i1);

                indices.push_back(i1);
                indices.push_back(i2);
                indices.push_back(i3);
            }
        }

        m_lodIndexCounts[lod] = static_cast<uint32_t>(indices.size());
        size_t bufferSize = sizeof(uint32_t) * indices.size();

        m_lodIndexBuffers[lod] = device.createBuffer(
            bufferSize,
            rhi::BufferUsage::Index | rhi::BufferUsage::TransferDst,
            rhi::MemoryUsage::GpuOnly
        );
        device.uploadBuffer(*m_lodIndexBuffers[lod], indices.data(), bufferSize);
    }
}

void ChunkManager::rebuildChunks(rhi::RHIDevice& device) {
    m_chunks.clear();
    m_chunks.reserve((2 * m_radius + 1) * (2 * m_radius + 1));

    for (int dz = -m_radius; dz <= m_radius; ++dz) {
        for (int dx = -m_radius; dx <= m_radius; ++dx) {
            m_chunks.emplace_back(device, m_centerChunkX + dx, m_centerChunkZ + dz);
        }
    }

    refreshChunkOrigins();
}

void ChunkManager::refreshChunkOrigins() {
    m_chunkOrigins.clear();
    m_chunkOrigins.reserve(m_chunks.size());
    for (const auto& chunk : m_chunks) {
        m_chunkOrigins.push_back(chunk.getWorldPos());
    }
}

void ChunkManager::setRadius(rhi::RHIDevice& device, int newRadius, const TerrainConfig& config) {
    if (newRadius < 1) newRadius = 1;
    if (newRadius > 8) newRadius = 8;
    if (m_radius == newRadius && !m_chunks.empty()) return;

    device.waitIdle();
    m_radius = newRadius;
    m_chunks.clear();

    rebuildChunks(device);
    regenerateAll(device, config);
}

void ChunkManager::update(rhi::RHIDevice& device, const glm::vec3& cameraPos, const TerrainConfig& config) {
    int targetCenterX = static_cast<int>(std::floor(cameraPos.x / CHUNK_SIZE));
    int targetCenterZ = static_cast<int>(std::floor(cameraPos.z / CHUNK_SIZE));

    bool centerShifted = (targetCenterX != m_centerChunkX || targetCenterZ != m_centerChunkZ);
    m_centerChunkX = targetCenterX;
    m_centerChunkZ = targetCenterZ;

    if (centerShifted) {
        size_t idx = 0;
        for (int dz = -m_radius; dz <= m_radius; ++dz) {
            for (int dx = -m_radius; dx <= m_radius; ++dx) {
                m_chunks[idx].setCoord(m_centerChunkX + dx, m_centerChunkZ + dz);
                idx++;
            }
        }
        refreshChunkOrigins();
    }

    m_dirtyChunks.clear();
    for (auto& chunk : m_chunks) {
        chunk.updateLOD(cameraPos, config.lodMode);
        if (chunk.needsRegeneration()) {
            m_dirtyChunks.push_back(&chunk);
        }
    }

    if (!m_dirtyChunks.empty()) {
        m_generator->generateChunks(device, m_dirtyChunks, config);
    }
}

void ChunkManager::regenerateAll(rhi::RHIDevice& device, const TerrainConfig& config) {
    m_dirtyChunks.clear();
    m_dirtyChunks.reserve(m_chunks.size());
    for (auto& chunk : m_chunks) {
        chunk.markDirty();
        m_dirtyChunks.push_back(&chunk);
    }
    m_generator->generateChunks(device, m_dirtyChunks, config);
}

void ChunkManager::recordRenderCommands(rhi::RHICommandList& cmd, const Frustum& frustum, const TerrainConfig& config) {
    float minY = -0.5f * config.amplitude;
    float maxY = 1.5f * config.amplitude;

    for (uint32_t lod = 0; lod < NUM_LOD_LEVELS; ++lod) {
        cmd.bindIndexBuffer(*m_lodIndexBuffers[lod]);

        uint32_t lodGridRes = getLODGridRes(lod);
        uint32_t lodStep = getLODStep(lod);

        for (const auto& chunk : m_chunks) {
            if (chunk.getLOD() != lod) continue;

            glm::vec2 wp = chunk.getWorldPos();
            glm::vec3 min(wp.x, minY, wp.y);
            glm::vec3 max(wp.x + CHUNK_SIZE, maxY, wp.y + CHUNK_SIZE);
            if (!frustum.intersectsAABB(min, max)) continue;

            cmd.bindStorageBuffer(0, chunk.getSSBOBuffer());

            ChunkPushConstants pc{};
            pc.chunkOffset = glm::vec4(
                wp.x,
                wp.y,
                CHUNK_CELL_SIZE,
                static_cast<float>(CHUNK_GRID_RES)
            );
            pc.lodParams = glm::uvec4(lodGridRes, lodStep, lod, 0);

            cmd.pushConstants(rhi::ShaderStage::Vertex, &pc, sizeof(ChunkPushConstants));
            cmd.drawIndexed(m_lodIndexCounts[lod], 1, 0);
        }
    }
}

void ChunkManager::recordShadowCommands(rhi::RHICommandList& cmd, uint32_t cascadeIndex, const TerrainConfig& config) {
    (void)config;
    uint32_t minLod = (cascadeIndex <= 1) ? 0u : (cascadeIndex == 2 ? 1u : 2u);

    for (uint32_t lod = minLod; lod < NUM_LOD_LEVELS; ++lod) {
        cmd.bindIndexBuffer(*m_lodIndexBuffers[lod]);
        uint32_t lodGridRes = getLODGridRes(lod);
        uint32_t lodStep = getLODStep(lod);

        for (const auto& chunk : m_chunks) {
            uint32_t effectiveLod = std::max(chunk.getLOD(), minLod);
            if (effectiveLod != lod) continue;

            glm::vec2 wp = chunk.getWorldPos();
            cmd.bindStorageBuffer(0, chunk.getSSBOBuffer());

            ChunkPushConstants pc{};
            pc.chunkOffset = glm::vec4(
                wp.x,
                wp.y,
                CHUNK_CELL_SIZE,
                static_cast<float>(CHUNK_GRID_RES)
            );
            pc.lodParams = glm::uvec4(lodGridRes, lodStep, lod, cascadeIndex);

            cmd.pushConstants(rhi::ShaderStage::Vertex, &pc, sizeof(ChunkPushConstants));
            cmd.drawIndexed(m_lodIndexCounts[lod], 1, 0);
        }
    }
}
