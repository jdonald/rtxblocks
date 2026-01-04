#pragma once
#include "MathUtils.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <vector>
#include <cstdint>

using Microsoft::WRL::ComPtr;

enum class MobType {
    Cow
};

class Mob {
public:
    Mob(MobType type, const Vector3& position);
    ~Mob();

    void Update(float deltaTime);
    void Render(ID3D11DeviceContext* context);

    void CreateMesh(ID3D11Device* device);

    Vector3 GetPosition() const { return m_position; }
    void SetPosition(const Vector3& pos) { m_position = pos; }

    Matrix4x4 GetWorldMatrix() const;

private:
    MobType m_type;
    Vector3 m_position;
    Vector3 m_velocity;
    float m_yaw;

    float m_wanderTimer;
    Vector3 m_wanderTarget;

    ComPtr<ID3D11Buffer> m_vertexBuffer;
    ComPtr<ID3D11Buffer> m_indexBuffer;
    uint32_t m_indexCount;

    void UpdateAI(float deltaTime);
    void CreateCowMesh(std::vector<struct Vertex>& vertices, std::vector<uint32_t>& indices);
};
