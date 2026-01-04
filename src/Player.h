#pragma once
#include "MathUtils.h"
#include "Block.h"
#include "Camera.h"
#include <array>

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

private:
    void UpdateMovement(float deltaTime, Window* window, World* world, SoundSystem* soundSystem);
    void UpdateLook(float deltaTime, Window* window);
    void UpdateBlockInteraction(Window* window, World* world, SoundSystem* soundSystem);

    Vector3 m_position;
    Vector3 m_velocity;
    float m_yaw;
    float m_pitch;
    Camera m_camera;

    bool m_isFlying;
    float m_moveSpeed;
    float m_flySpeed;
    float m_mouseSensitivity;

    std::array<BlockType, 9> m_inventory;
    int m_selectedSlot;

    bool m_leftClickPressed;
    bool m_rightClickPressed;

    // For landing sounds
    bool m_wasOnGround;
    float m_fallDistance;
};
