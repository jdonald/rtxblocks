#include "Chunk.h"
#include "BlockDatabase.h"
#include <cstring>

Chunk::Chunk(int chunkX, int chunkZ)
    : m_chunkX(chunkX)
    , m_chunkZ(chunkZ)
    , m_needsMeshUpdate(true)
    , m_isEmpty(true)
    , m_indexCount(0) {
    std::memset(m_blocks, 0, sizeof(m_blocks));
}

Chunk::~Chunk() {
}

void Chunk::SetBlock(int x, int y, int z, BlockType type) {
    if (!IsBlockInBounds(x, y, z)) return;

    m_blocks[x][y][z] = Block(type);
    m_needsMeshUpdate = true;

    if (type != BlockType::Air) {
        m_isEmpty = false;
    }
}

Block Chunk::GetBlock(int x, int y, int z) const {
    if (!IsBlockInBounds(x, y, z)) {
        return Block(BlockType::Air);
    }
    return m_blocks[x][y][z];
}

bool Chunk::IsBlockInBounds(int x, int y, int z) const {
    return x >= 0 && x < CHUNK_SIZE &&
           y >= 0 && y < CHUNK_HEIGHT &&
           z >= 0 && z < CHUNK_SIZE;
}

bool Chunk::ShouldRenderFace(int x, int y, int z, BlockFace::Face face) const {
    Block currentBlock = GetBlock(x, y, z);
    if (currentBlock.IsAir()) return false;

    int nx = x, ny = y, nz = z;

    switch (face) {
    case BlockFace::Front:  nz++; break;
    case BlockFace::Back:   nz--; break;
    case BlockFace::Left:   nx--; break;
    case BlockFace::Right:  nx++; break;
    case BlockFace::Top:    ny++; break;
    case BlockFace::Bottom: ny--; break;
    }

    // If neighbor is out of bounds, render the face
    if (!IsBlockInBounds(nx, ny, nz)) {
        return true;
    }

    Block neighbor = GetBlock(nx, ny, nz);

    // Don't render face if neighbor is opaque
    if (!neighbor.IsTransparent()) {
        return false;
    }

    // Render if neighbor is air or different transparent block
    return neighbor.IsAir() || neighbor.type != currentBlock.type;
}

void Chunk::AddBlockFace(const Vector3& pos, BlockFace::Face face, const Vector4& color) {
    Vector3 normals[6] = {
        Vector3(0, 0, 1),   // Front
        Vector3(0, 0, -1),  // Back
        Vector3(-1, 0, 0),  // Left
        Vector3(1, 0, 0),   // Right
        Vector3(0, 1, 0),   // Top
        Vector3(0, -1, 0)   // Bottom
    };

    Vector3 vertices[6][4] = {
        // Front
        {
            Vector3(0, 0, 1), Vector3(1, 0, 1), Vector3(1, 1, 1), Vector3(0, 1, 1)
        },
        // Back
        {
            Vector3(1, 0, 0), Vector3(0, 0, 0), Vector3(0, 1, 0), Vector3(1, 1, 0)
        },
        // Left
        {
            Vector3(0, 0, 0), Vector3(0, 0, 1), Vector3(0, 1, 1), Vector3(0, 1, 0)
        },
        // Right
        {
            Vector3(1, 0, 1), Vector3(1, 0, 0), Vector3(1, 1, 0), Vector3(1, 1, 1)
        },
        // Top
        {
            Vector3(0, 1, 1), Vector3(1, 1, 1), Vector3(1, 1, 0), Vector3(0, 1, 0)
        },
        // Bottom
        {
            Vector3(0, 0, 0), Vector3(1, 0, 0), Vector3(1, 0, 1), Vector3(0, 0, 1)
        }
    };

    Vector2 texCoords[4] = {
        Vector2(0, 1), Vector2(1, 1), Vector2(1, 0), Vector2(0, 0)
    };

    uint32_t baseIndex = static_cast<uint32_t>(m_vertices.size());
    Vector3 normal = normals[face];

    for (int i = 0; i < 4; i++) {
        Vertex v;
        v.position = pos + vertices[face][i];
        v.normal = normal;
        v.color = color;
        v.texCoord = texCoords[i];
        m_vertices.push_back(v);
    }

    m_indices.push_back(baseIndex);
    m_indices.push_back(baseIndex + 1);
    m_indices.push_back(baseIndex + 2);
    m_indices.push_back(baseIndex);
    m_indices.push_back(baseIndex + 2);
    m_indices.push_back(baseIndex + 3);
}

void Chunk::GenerateMesh() {
    m_vertices.clear();
    m_indices.clear();

    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int y = 0; y < CHUNK_HEIGHT; y++) {
            for (int z = 0; z < CHUNK_SIZE; z++) {
                Block block = GetBlock(x, y, z);
                if (block.IsAir()) continue;

                Vector3 blockPos(x, y, z);
                const BlockProperties& props = BlockDatabase::GetProperties(block.type);

                for (int face = 0; face < 6; face++) {
                    if (ShouldRenderFace(x, y, z, static_cast<BlockFace::Face>(face))) {
                        AddBlockFace(blockPos, static_cast<BlockFace::Face>(face), props.color);
                    }
                }
            }
        }
    }

    m_needsMeshUpdate = false;
}

void Chunk::UpdateBuffer(ID3D11Device* device) {
    if (m_vertices.empty()) {
        m_indexCount = 0;
        return;
    }

    // Create vertex buffer
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.ByteWidth = static_cast<UINT>(m_vertices.size() * sizeof(Vertex));
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = m_vertices.data();

    device->CreateBuffer(&vbDesc, &vbData, m_vertexBuffer.ReleaseAndGetAddressOf());

    // Create index buffer
    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.ByteWidth = static_cast<UINT>(m_indices.size() * sizeof(uint32_t));
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = m_indices.data();

    device->CreateBuffer(&ibDesc, &ibData, m_indexBuffer.ReleaseAndGetAddressOf());

    m_indexCount = static_cast<uint32_t>(m_indices.size());
}

void Chunk::Render(ID3D11DeviceContext* context) {
    if (!m_vertexBuffer || !m_indexBuffer || m_indexCount == 0) {
        return;
    }

    UINT stride = sizeof(Vertex);
    UINT offset = 0;

    context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
    context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    context->DrawIndexed(m_indexCount, 0, 0);
}
