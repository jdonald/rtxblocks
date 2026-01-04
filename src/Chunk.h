#pragma once
#include "Block.h"
#include "MathUtils.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <vector>
#include <cstdint>

using Microsoft::WRL::ComPtr;

const int CHUNK_SIZE = 16;
const int CHUNK_HEIGHT = 256;

struct Vertex {
    Vector3 position;
    Vector3 normal;
    Vector4 color;
    Vector2 texCoord;
};

class Chunk {
public:
    Chunk(int chunkX, int chunkZ);
    ~Chunk();

    void SetBlock(int x, int y, int z, BlockType type);
    Block GetBlock(int x, int y, int z) const;

    bool IsBlockInBounds(int x, int y, int z) const;

    void GenerateMesh();
    void UpdateBuffer(ID3D11Device* device);

    void Render(ID3D11DeviceContext* context);
    void RenderTransparent(ID3D11DeviceContext* context);

    bool NeedsMeshUpdate() const { return m_needsMeshUpdate; }
    void MarkForMeshUpdate() { m_needsMeshUpdate = true; m_needsBufferUpdate = true; }
    bool NeedsBufferUpdate() const { return m_needsBufferUpdate; }

    int GetChunkX() const { return m_chunkX; }
    int GetChunkZ() const { return m_chunkZ; }

    uint32_t GetSolidIndexCount() const { return m_indexCount; }
    uint32_t GetTransparentIndexCount() const { return m_transparentIndexCount; }

    Vector3 GetWorldPosition() const {
        return Vector3(m_chunkX * CHUNK_SIZE, 0, m_chunkZ * CHUNK_SIZE);
    }

    bool IsEmpty() const { return m_isEmpty; }

private:
    void AddBlockFace(const Vector3& pos, BlockFace::Face face, const Vector4& color, bool isTransparent);
    bool ShouldRenderFace(int x, int y, int z, BlockFace::Face face) const;

    int m_chunkX, m_chunkZ;
    Block m_blocks[CHUNK_SIZE][CHUNK_HEIGHT][CHUNK_SIZE];
    
    // Solid geometry
    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;
    ComPtr<ID3D11Buffer> m_vertexBuffer;
    ComPtr<ID3D11Buffer> m_indexBuffer;
    uint32_t m_indexCount;

    // Transparent geometry
    std::vector<Vertex> m_transparentVertices;
    std::vector<uint32_t> m_transparentIndices;
    ComPtr<ID3D11Buffer> m_transparentVertexBuffer;
    ComPtr<ID3D11Buffer> m_transparentIndexBuffer;
    uint32_t m_transparentIndexCount;

    bool m_needsMeshUpdate;
    bool m_needsBufferUpdate;
    bool m_isEmpty;
};
