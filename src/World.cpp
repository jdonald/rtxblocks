#include "World.h"
#include <cmath>
#include <algorithm>
#include <vector>

World::World(unsigned int seed)
    : m_terrainGenerator(seed)
    , m_renderDistance(8) {
}

World::~World() {
}

void World::GetChunkAndLocalCoords(int worldX, int worldZ, int& chunkX, int& chunkZ, int& localX, int& localZ) const {
    chunkX = static_cast<int>(std::floor(static_cast<float>(worldX) / CHUNK_SIZE));
    chunkZ = static_cast<int>(std::floor(static_cast<float>(worldZ) / CHUNK_SIZE));

    localX = worldX - chunkX * CHUNK_SIZE;
    localZ = worldZ - chunkZ * CHUNK_SIZE;

    if (localX < 0) localX += CHUNK_SIZE;
    if (localZ < 0) localZ += CHUNK_SIZE;
}

Chunk* World::GetChunk(int chunkX, int chunkZ) {
    auto key = std::make_pair(chunkX, chunkZ);
    auto it = m_chunks.find(key);
    if (it != m_chunks.end()) {
        return it->second.get();
    }
    return nullptr;
}

Chunk* World::GetOrCreateChunk(int chunkX, int chunkZ) {
    Chunk* chunk = GetChunk(chunkX, chunkZ);
    if (chunk) {
        return chunk;
    }

    auto newChunk = std::make_unique<Chunk>(chunkX, chunkZ);
    m_terrainGenerator.GenerateChunk(newChunk.get());
    newChunk->GenerateMesh();
    // Note: Buffer creation will happen in World::Update when device is available

    Chunk* chunkPtr = newChunk.get();
    m_chunks[std::make_pair(chunkX, chunkZ)] = std::move(newChunk);

    return chunkPtr;
}

void World::Update(const Vector3& playerPos, ID3D11Device* device) {
    int playerChunkX = static_cast<int>(std::floor(playerPos.x / CHUNK_SIZE));
    int playerChunkZ = static_cast<int>(std::floor(playerPos.z / CHUNK_SIZE));

    // Load chunks around player
    for (int dx = -m_renderDistance; dx <= m_renderDistance; dx++) {
        for (int dz = -m_renderDistance; dz <= m_renderDistance; dz++) {
            int chunkX = playerChunkX + dx;
            int chunkZ = playerChunkZ + dz;

            Chunk* chunk = GetOrCreateChunk(chunkX, chunkZ);

            // Always update buffer if needed (including first creation)
            if (chunk->NeedsMeshUpdate()) {
                chunk->GenerateMesh();
            }

            // Update GPU buffers (this checks internally if needed)
            chunk->UpdateBuffer(device);
        }
    }

    // Unload distant chunks
    std::vector<std::pair<int, int>> chunksToRemove;
    for (auto& pair : m_chunks) {
        int chunkX = pair.first.first;
        int chunkZ = pair.first.second;

        int dx = chunkX - playerChunkX;
        int dz = chunkZ - playerChunkZ;

        if (std::abs(dx) > m_renderDistance + 2 || std::abs(dz) > m_renderDistance + 2) {
            chunksToRemove.push_back(pair.first);
        }
    }

    for (const auto& key : chunksToRemove) {
        m_chunks.erase(key);
    }
}

void World::Render(ID3D11DeviceContext* context) {
    for (auto& pair : m_chunks) {
        pair.second->Render(context);
    }
}

void World::RenderTransparent(ID3D11DeviceContext* context, const Vector3& cameraPos) {
    struct ChunkEntry {
        Chunk* chunk;
        float distanceSq;
    };

    std::vector<ChunkEntry> entries;
    entries.reserve(m_chunks.size());

    for (const auto& pair : m_chunks) {
        Chunk* chunk = pair.second.get();
        Vector3 chunkPos = chunk->GetWorldPosition();
        Vector3 center(chunkPos.x + CHUNK_SIZE * 0.5f, CHUNK_HEIGHT * 0.5f, chunkPos.z + CHUNK_SIZE * 0.5f);
        Vector3 delta = center - cameraPos;
        float distSq = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
        entries.push_back({ chunk, distSq });
    }

    std::sort(entries.begin(), entries.end(),
              [](const ChunkEntry& a, const ChunkEntry& b) { return a.distanceSq > b.distanceSq; });

    for (const auto& entry : entries) {
        entry.chunk->RenderTransparent(context);
    }
}

Block World::GetBlock(int worldX, int worldY, int worldZ) const {
    if (worldY < 0 || worldY >= CHUNK_HEIGHT) {
        return Block(BlockType::Air);
    }

    int chunkX, chunkZ, localX, localZ;
    GetChunkAndLocalCoords(worldX, worldZ, chunkX, chunkZ, localX, localZ);

    auto it = m_chunks.find(std::make_pair(chunkX, chunkZ));
    if (it != m_chunks.end()) {
        return it->second->GetBlock(localX, worldY, localZ);
    }

    return Block(BlockType::Air);
}

void World::SetBlock(int worldX, int worldY, int worldZ, BlockType type) {
    if (worldY < 0 || worldY >= CHUNK_HEIGHT) {
        return;
    }

    int chunkX, chunkZ, localX, localZ;
    GetChunkAndLocalCoords(worldX, worldZ, chunkX, chunkZ, localX, localZ);

    Chunk* chunk = GetChunk(chunkX, chunkZ);
    if (chunk) {
        chunk->SetBlock(localX, worldY, localZ, type);
    }
}

int World::GetTerrainHeight(int worldX, int worldZ) const {
    return m_terrainGenerator.GetTerrainHeight(worldX, worldZ);
}

World::DebugStats World::GetDebugStats() const {
    DebugStats stats;
    stats.chunkCount = static_cast<int>(m_chunks.size());

    for (const auto& pair : m_chunks) {
        const Chunk* chunk = pair.second.get();
        stats.solidIndexCount += static_cast<uint64_t>(chunk->GetSolidIndexCount());
        stats.transparentIndexCount += static_cast<uint64_t>(chunk->GetTransparentIndexCount());
    }

    return stats;
}

void World::GatherSolidMesh(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices) const {
    vertices.clear();
    indices.clear();

    for (const auto& pair : m_chunks) {
        const Chunk* chunk = pair.second.get();
        const auto& chunkVerts = chunk->GetSolidVertices();
        const auto& chunkIndices = chunk->GetSolidIndices();
        if (chunkVerts.empty() || chunkIndices.empty()) {
            continue;
        }

        uint32_t baseIndex = static_cast<uint32_t>(vertices.size());
        vertices.insert(vertices.end(), chunkVerts.begin(), chunkVerts.end());
        indices.reserve(indices.size() + chunkIndices.size());
        for (uint32_t idx : chunkIndices) {
            indices.push_back(baseIndex + idx);
        }
    }
}

void World::GatherTransparentMesh(const Vector3& cameraPos, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices) const {
    vertices.clear();
    indices.clear();

    struct ChunkEntry {
        Chunk* chunk;
        float distanceSq;
    };

    std::vector<ChunkEntry> entries;
    entries.reserve(m_chunks.size());

    for (const auto& pair : m_chunks) {
        Chunk* chunk = pair.second.get();
        Vector3 chunkPos = chunk->GetWorldPosition();
        Vector3 center(chunkPos.x + CHUNK_SIZE * 0.5f, CHUNK_HEIGHT * 0.5f, chunkPos.z + CHUNK_SIZE * 0.5f);
        Vector3 delta = center - cameraPos;
        float distSq = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
        entries.push_back({ chunk, distSq });
    }

    std::sort(entries.begin(), entries.end(),
              [](const ChunkEntry& a, const ChunkEntry& b) { return a.distanceSq > b.distanceSq; });

    for (const auto& entry : entries) {
        const auto& chunkVerts = entry.chunk->GetTransparentVertices();
        const auto& chunkIndices = entry.chunk->GetTransparentIndices();
        if (chunkVerts.empty() || chunkIndices.empty()) {
            continue;
        }

        uint32_t baseIndex = static_cast<uint32_t>(vertices.size());
        vertices.insert(vertices.end(), chunkVerts.begin(), chunkVerts.end());
        indices.reserve(indices.size() + chunkIndices.size());
        for (uint32_t idx : chunkIndices) {
            indices.push_back(baseIndex + idx);
        }
    }
}

bool World::Raycast(const Vector3& origin, const Vector3& direction, float maxDistance,
                    Vector3& hitPos, Vector3& hitNormal, Block& hitBlock) {
    Vector3 pos = origin;
    Vector3 step = direction.normalized() * 0.1f;

    for (float dist = 0; dist < maxDistance; dist += 0.1f) {
        pos = pos + step;

        int blockX = static_cast<int>(std::floor(pos.x));
        int blockY = static_cast<int>(std::floor(pos.y));
        int blockZ = static_cast<int>(std::floor(pos.z));

        Block block = GetBlock(blockX, blockY, blockZ);

        if (!block.IsAir() && !block.IsTransparent()) {
            hitPos = Vector3(blockX, blockY, blockZ);
            hitBlock = block;

            // Calculate normal based on which face was hit
            Vector3 blockCenter(blockX + 0.5f, blockY + 0.5f, blockZ + 0.5f);
            Vector3 diff = pos - blockCenter;

            float maxComponent = std::max({std::abs(diff.x), std::abs(diff.y), std::abs(diff.z)});

            if (std::abs(diff.x) == maxComponent) {
                hitNormal = Vector3(diff.x > 0 ? 1 : -1, 0, 0);
            } else if (std::abs(diff.y) == maxComponent) {
                hitNormal = Vector3(0, diff.y > 0 ? 1 : -1, 0);
            } else {
                hitNormal = Vector3(0, 0, diff.z > 0 ? 1 : -1);
            }

            return true;
        }
    }

    return false;
}
