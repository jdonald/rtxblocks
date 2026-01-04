#include "Mob.h"
#include "Chunk.h"
#include <random>
#include <cmath>

Mob::Mob(MobType type, const Vector3& position)
    : m_type(type)
    , m_position(position)
    , m_velocity(0, 0, 0)
    , m_yaw(0)
    , m_wanderTimer(0)
    , m_wanderTarget(position)
    , m_indexCount(0) {
}

Mob::~Mob() {
}

void Mob::Update(float deltaTime) {
    UpdateAI(deltaTime);

    // Apply movement
    m_position = m_position + m_velocity * deltaTime;

    // Simple ground detection
    if (m_position.y < 70.0f) {
        m_position.y = 70.0f;
        m_velocity.y = 0;
    }
}

void Mob::UpdateAI(float deltaTime) {
    // Simple wandering AI
    m_wanderTimer -= deltaTime;

    if (m_wanderTimer <= 0) {
        // Pick a new wander target
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_real_distribution<float> dis(-10.0f, 10.0f);

        m_wanderTarget = m_position + Vector3(dis(gen), 0, dis(gen));
        m_wanderTimer = 3.0f + dis(gen) * 0.5f;
    }

    // Move towards wander target
    Vector3 toTarget = m_wanderTarget - m_position;
    toTarget.y = 0; // Don't move vertically

    float distance = toTarget.length();
    if (distance > 0.5f) {
        Vector3 direction = toTarget / distance;
        m_velocity = direction * 2.0f; // Slow walking speed

        // Update yaw to face movement direction
        m_yaw = std::atan2(direction.x, direction.z);
    } else {
        m_velocity = Vector3(0, 0, 0);
    }
}

void Mob::CreateCowMesh(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices) {
    // Simple cow model - just a body and head using boxes
    Vector4 cowColor(0.5f, 0.3f, 0.2f, 1.0f); // Brown

    auto addBox = [&](const Vector3& center, const Vector3& size, const Vector4& color) {
        Vector3 min = center - size * 0.5f;
        Vector3 max = center + size * 0.5f;

        uint32_t baseIdx = static_cast<uint32_t>(vertices.size());

        // 8 vertices of the box
        Vector3 corners[8] = {
            Vector3(min.x, min.y, min.z),
            Vector3(max.x, min.y, min.z),
            Vector3(max.x, max.y, min.z),
            Vector3(min.x, max.y, min.z),
            Vector3(min.x, min.y, max.z),
            Vector3(max.x, min.y, max.z),
            Vector3(max.x, max.y, max.z),
            Vector3(min.x, max.y, max.z)
        };

        // Add vertices with normals for each face
        struct Face {
            int indices[4];
            Vector3 normal;
        };

        Face faces[6] = {
            {{0, 1, 2, 3}, Vector3(0, 0, -1)}, // Front
            {{5, 4, 7, 6}, Vector3(0, 0, 1)},  // Back
            {{4, 0, 3, 7}, Vector3(-1, 0, 0)}, // Left
            {{1, 5, 6, 2}, Vector3(1, 0, 0)},  // Right
            {{3, 2, 6, 7}, Vector3(0, 1, 0)},  // Top
            {{4, 5, 1, 0}, Vector3(0, -1, 0)}  // Bottom
        };

        for (int f = 0; f < 6; f++) {
            uint32_t faceBase = static_cast<uint32_t>(vertices.size());

            for (int i = 0; i < 4; i++) {
                Vertex v;
                v.position = corners[faces[f].indices[i]];
                v.normal = faces[f].normal;
                v.color = color;
                v.texCoord = Vector2(i % 2, i / 2);
                vertices.push_back(v);
            }

            indices.push_back(faceBase);
            indices.push_back(faceBase + 1);
            indices.push_back(faceBase + 2);
            indices.push_back(faceBase);
            indices.push_back(faceBase + 2);
            indices.push_back(faceBase + 3);
        }
    };

    // Body
    addBox(Vector3(0, 0.6f, 0), Vector3(1.0f, 0.8f, 1.5f), cowColor);

    // Head
    addBox(Vector3(0, 0.8f, 0.9f), Vector3(0.6f, 0.6f, 0.6f), cowColor);

    // Legs
    addBox(Vector3(-0.3f, 0.2f, -0.5f), Vector3(0.2f, 0.4f, 0.2f), cowColor);
    addBox(Vector3(0.3f, 0.2f, -0.5f), Vector3(0.2f, 0.4f, 0.2f), cowColor);
    addBox(Vector3(-0.3f, 0.2f, 0.5f), Vector3(0.2f, 0.4f, 0.2f), cowColor);
    addBox(Vector3(0.3f, 0.2f, 0.5f), Vector3(0.2f, 0.4f, 0.2f), cowColor);
}

void Mob::CreateMesh(ID3D11Device* device) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    if (m_type == MobType::Cow) {
        CreateCowMesh(vertices, indices);
    }

    if (vertices.empty()) return;

    // Create vertex buffer
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(Vertex));
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = vertices.data();

    device->CreateBuffer(&vbDesc, &vbData, m_vertexBuffer.ReleaseAndGetAddressOf());

    // Create index buffer
    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.ByteWidth = static_cast<UINT>(indices.size() * sizeof(uint32_t));
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = indices.data();

    device->CreateBuffer(&ibDesc, &ibData, m_indexBuffer.ReleaseAndGetAddressOf());

    m_indexCount = static_cast<uint32_t>(indices.size());
}

void Mob::Render(ID3D11DeviceContext* context) {
    if (!m_vertexBuffer || !m_indexBuffer) return;

    UINT stride = sizeof(Vertex);
    UINT offset = 0;

    context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
    context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    context->DrawIndexed(m_indexCount, 0, 0);
}

Matrix4x4 Mob::GetWorldMatrix() const {
    Matrix4x4 rotation = Matrix4x4::RotationY(m_yaw);
    Matrix4x4 translation = Matrix4x4::Translation(m_position.x, m_position.y, m_position.z);
    return rotation * translation;
}
