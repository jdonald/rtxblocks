#pragma once
#include "Chunk.h"
#include "TerrainGenerator.h"
#include "MathUtils.h"
#include <map>
#include <memory>
#include <cstdint>
#include <vector>

class World {
public:
    World(unsigned int seed = 12345);
    ~World();

    struct DebugStats {
        int chunkCount = 0;
        uint64_t solidIndexCount = 0;
        uint64_t transparentIndexCount = 0;
    };

    void Update(const Vector3& playerPos, ID3D11Device* device);
    void Render(ID3D11DeviceContext* context);
    void RenderTransparent(ID3D11DeviceContext* context, const Vector3& cameraPos);

    Block GetBlock(int worldX, int worldY, int worldZ) const;
    void SetBlock(int worldX, int worldY, int worldZ, BlockType type);
    int GetTerrainHeight(int worldX, int worldZ) const;
    DebugStats GetDebugStats() const;
    void GatherSolidMesh(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices) const;
    void GatherTransparentMesh(const Vector3& cameraPos, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices) const;

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
