#include "TerrainGenerator.h"
#include "Chunk.h"
#include <cmath>

TerrainGenerator::TerrainGenerator(unsigned int seed)
    : m_heightNoise(seed)
    , m_moistureNoise(seed + 1)
    , m_treeNoise(seed + 2)
    , m_seed(seed) {
}

int TerrainGenerator::GetTerrainHeight(int worldX, int worldZ) const {
    float scale = 0.01f;
    float noiseValue = m_heightNoise.OctaveNoise(worldX * scale, worldZ * scale, 4, 0.5f);

    // Map from [-1, 1] to [40, 100]
    int height = static_cast<int>(40 + (noiseValue * 0.5f + 0.5f) * 60);

    return height;
}

float TerrainGenerator::GetMoisture(int worldX, int worldZ) const {
    float scale = 0.02f;
    float noiseValue = m_moistureNoise.OctaveNoise(worldX * scale, worldZ * scale, 3, 0.5f);
    return noiseValue * 0.5f + 0.5f; // Map to [0, 1]
}

void TerrainGenerator::GenerateChunk(Chunk* chunk) {
    GenerateTerrain(chunk);
    GenerateTrees(chunk);
    GenerateWater(chunk);
}

void TerrainGenerator::GenerateTerrain(Chunk* chunk) {
    Vector3 chunkWorldPos = chunk->GetWorldPosition();

    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {
            int worldX = static_cast<int>(chunkWorldPos.x) + x;
            int worldZ = static_cast<int>(chunkWorldPos.z) + z;

            int height = GetTerrainHeight(worldX, worldZ);

            // Generate stone base
            for (int y = 0; y < height - 4 && y < CHUNK_HEIGHT; y++) {
                chunk->SetBlock(x, y, z, BlockType::Stone);
            }

            // Dirt layer
            for (int y = height - 4; y < height && y < CHUNK_HEIGHT; y++) {
                if (y >= 0) {
                    chunk->SetBlock(x, y, z, BlockType::Dirt);
                }
            }

            // Top layer - grass represented as dirt for now
            if (height >= 0 && height < CHUNK_HEIGHT) {
                chunk->SetBlock(x, height, z, BlockType::Dirt);
            }
        }
    }
}

void TerrainGenerator::GenerateTrees(Chunk* chunk) {
    Vector3 chunkWorldPos = chunk->GetWorldPosition();

    for (int x = 2; x < CHUNK_SIZE - 2; x += 4) {
        for (int z = 2; z < CHUNK_SIZE - 2; z += 4) {
            int worldX = static_cast<int>(chunkWorldPos.x) + x;
            int worldZ = static_cast<int>(chunkWorldPos.z) + z;

            float treeChance = m_treeNoise.Noise(worldX * 0.1f, worldZ * 0.1f);
            float moisture = GetMoisture(worldX, worldZ);

            // More trees in moist areas
            if (treeChance > 0.3f && moisture > 0.4f) {
                int groundHeight = GetTerrainHeight(worldX, worldZ);
                PlaceTree(chunk, x, groundHeight + 1, z);
            }
        }
    }
}

void TerrainGenerator::PlaceTree(Chunk* chunk, int x, int y, int z) {
    int trunkHeight = 5;

    // Trunk
    for (int i = 0; i < trunkHeight && (y + i) < CHUNK_HEIGHT; i++) {
        chunk->SetBlock(x, y + i, z, BlockType::Wood);
    }

    // Leaves - simple spherical shape
    int leafY = y + trunkHeight;
    for (int dx = -2; dx <= 2; dx++) {
        for (int dy = -2; dy <= 2; dy++) {
            for (int dz = -2; dz <= 2; dz++) {
                int lx = x + dx;
                int ly = leafY + dy;
                int lz = z + dz;

                // Simple distance check for spherical leaves
                if (dx * dx + dy * dy + dz * dz <= 8) {
                    if (chunk->IsBlockInBounds(lx, ly, lz)) {
                        Block existingBlock = chunk->GetBlock(lx, ly, lz);
                        if (existingBlock.IsAir()) {
                            chunk->SetBlock(lx, ly, lz, BlockType::Leaves);
                        }
                    }
                }
            }
        }
    }
}

void TerrainGenerator::GenerateWater(Chunk* chunk) {
    Vector3 chunkWorldPos = chunk->GetWorldPosition();
    int waterLevel = 60;

    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {
            int worldX = static_cast<int>(chunkWorldPos.x) + x;
            int worldZ = static_cast<int>(chunkWorldPos.z) + z;

            int terrainHeight = GetTerrainHeight(worldX, worldZ);

            // Fill below water level with water
            if (terrainHeight < waterLevel) {
                for (int y = terrainHeight + 1; y <= waterLevel && y < CHUNK_HEIGHT; y++) {
                    if (chunk->GetBlock(x, y, z).IsAir()) {
                        chunk->SetBlock(x, y, z, BlockType::Water);
                    }
                }
            }
        }
    }
}
