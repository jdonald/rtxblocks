#pragma once
#include "Chunk.h"
#include "TerrainGenerator.h"
#include "MathUtils.h"
#include <map>
#include <memory>

class World {
public:
    World(unsigned int seed = 12345);
    ~World();

    void Update(const Vector3& playerPos, ID3D11Device* device);
    void Render(ID3D11DeviceContext* context);
    void RenderTransparent(ID3D11DeviceContext* context);

    Block GetBlock(int worldX, int worldY, int worldZ) const;
    void SetBlock(int worldX, int worldY, int worldZ, BlockType type);

    bool Raycast(const Vector3& origin, const Vector3& direction, float maxDistance,
                 Vector3& hitPos, Vector3& hitNormal, Block& hitBlock);

private:
    Chunk* GetChunk(int chunkX, int chunkZ);
    Chunk* GetOrCreateChunk(int chunkX, int chunkZ);
    void GetChunkAndLocalCoords(int worldX, int worldZ, int& chunkX, int& chunkZ, int& localX, int& localZ) const;

    std::map<std::pair<int, int>, std::unique_ptr<Chunk>> m_chunks;
    TerrainGenerator m_terrainGenerator;
    int m_renderDistance;
};
