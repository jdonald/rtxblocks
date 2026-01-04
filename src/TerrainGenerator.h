#pragma once
#include "PerlinNoise.h"
#include "Block.h"

class Chunk;

class TerrainGenerator {
public:
    TerrainGenerator(unsigned int seed = 12345);

    void GenerateChunk(Chunk* chunk);

private:
    void GenerateTerrain(Chunk* chunk);
    void GenerateTrees(Chunk* chunk);
    void GenerateWater(Chunk* chunk);

    void PlaceTree(Chunk* chunk, int worldX, int worldY, int worldZ);

    int GetTerrainHeight(int worldX, int worldZ) const;
    float GetMoisture(int worldX, int worldZ) const;

    PerlinNoise m_heightNoise;
    PerlinNoise m_moistureNoise;
    PerlinNoise m_treeNoise;

    unsigned int m_seed;
};
