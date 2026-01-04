#pragma once
#include "MathUtils.h"
#include "Block.h"
#include "Camera.h"
#include "Chunk.h"
#include <array>
#include <d3d11.h>
#include <wrl/client.h>

class Window;
class World;
class SoundSystem;

class Player {
public:
    Player();

    void Update(float deltaTime, Window* window, World* world, SoundSystem* soundSystem = nullptr);

    Vector3 GetPosition() const { return m_position; }
    void SetPosition(const Vector3& pos) { m_position = pos; }

    Camera& GetCamera() { return m_camera; }

    BlockType GetSelectedBlock() const;
    void SetSelectedSlot(int slot);
    int GetSelectedSlot() const { return m_selectedSlot; }

    void CreateMesh(ID3D11Device* device);
    void Render(ID3D11DeviceContext* context);
    Matrix4x4 GetWorldMatrix() const;

private:
    void UpdateMovement(float deltaTime, Window* window, World* world, SoundSystem* soundSystem);
    void UpdateLook(float deltaTime, Window* window);
    void UpdateBlockInteraction(Window* window, World* world, SoundSystem* soundSystem);
    void AddBlockToInventory(BlockType type);

    Vector3 m_position;
    Vector3 m_velocity;
    float m_verticalVelocity;
    float m_yaw;
    float m_pitch;
    Camera m_camera;

    bool m_isFlying;
    bool m_onGround;
    float m_moveSpeed;
    float m_flySpeed;
    float m_mouseSensitivity;
    float m_gravity;
    float m_jumpVelocity;
    float m_spaceTapTimer;
    float m_doubleTapWindow;

    std::array<BlockType, 9> m_inventory;
    int m_selectedSlot;

    bool m_leftClickPressed;
    bool m_rightClickPressed;

    // For landing sounds
    bool m_wasOnGround;
    float m_fallDistance;

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_indexBuffer;
    uint32_t m_indexCount;
};
