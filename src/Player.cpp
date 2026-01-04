#include "Player.h"
#include "Window.h"
#include "World.h"
#include "SoundSystem.h"
#include <algorithm>

const float PI = 3.14159265359f;

Player::Player()
    : m_position(0, 100, 0)
    , m_velocity(0, 0, 0)
    , m_yaw(0)
    , m_pitch(0)
    , m_isFlying(true) // Creative mode - always flying
    , m_moveSpeed(10.0f)
    , m_flySpeed(20.0f)
    , m_mouseSensitivity(0.002f)
    , m_selectedSlot(0)
    , m_leftClickPressed(false)
    , m_rightClickPressed(false)
    , m_wasOnGround(false)
    , m_fallDistance(0.0f) {

    // Initialize inventory with available blocks
    m_inventory[0] = BlockType::Dirt;
    m_inventory[1] = BlockType::Stone;
    m_inventory[2] = BlockType::Wood;
    m_inventory[3] = BlockType::Leaves;
    m_inventory[4] = BlockType::Water;
    m_inventory[5] = BlockType::Torch;
    m_inventory[6] = BlockType::Air;
    m_inventory[7] = BlockType::Air;
    m_inventory[8] = BlockType::Air;
}

void Player::Update(float deltaTime, Window* window, World* world, SoundSystem* soundSystem) {
    UpdateLook(deltaTime, window);
    UpdateMovement(deltaTime, window, world, soundSystem);
    UpdateBlockInteraction(window, world, soundSystem);

    // Update camera
    m_camera.SetPosition(m_position);
    m_camera.SetRotation(m_yaw, m_pitch);
    m_camera.SetTargetPosition(m_position);

    // F5 to toggle camera mode
    if (window->WasKeyPressed(VK_F5)) {
        m_camera.ToggleMode();
    }

    // Number keys for inventory selection
    for (int i = 0; i < 9; i++) {
        if (window->WasKeyPressed('1' + i)) {
            m_selectedSlot = i;
        }
    }
}

void Player::UpdateLook(float deltaTime, Window* window) {
    int dx, dy;
    window->GetMouseDelta(dx, dy);

    m_yaw += dx * m_mouseSensitivity;
    m_pitch -= dy * m_mouseSensitivity;

    // Clamp pitch
    const float maxPitch = 89.0f * PI / 180.0f;
    m_pitch = std::max(-maxPitch, std::min(maxPitch, m_pitch));
}

void Player::UpdateMovement(float deltaTime, Window* window, World* world, SoundSystem* soundSystem) {
    Vector3 forward = m_camera.GetForward();
    Vector3 right = m_camera.GetRight();
    Vector3 up(0, 1, 0);

    // For movement, use horizontal forward direction
    Vector3 moveForward = Vector3(forward.x, 0, forward.z).normalized();
    Vector3 moveRight = Vector3(right.x, 0, right.z).normalized();

    Vector3 oldPos = m_position;
    Vector3 moveDir(0, 0, 0);

    if (window->IsKeyDown('W')) {
        moveDir = moveDir + moveForward;
    }
    if (window->IsKeyDown('S')) {
        moveDir = moveDir - moveForward;
    }
    if (window->IsKeyDown('A')) {
        moveDir = moveDir - moveRight;
    }
    if (window->IsKeyDown('D')) {
        moveDir = moveDir + moveRight;
    }

    // Creative mode flying
    if (window->IsKeyDown(VK_SPACE)) {
        moveDir = moveDir + up;
    }
    if (window->IsKeyDown(VK_SHIFT)) {
        moveDir = moveDir - up;
    }

    // Normalize and apply speed
    float length = moveDir.length();
    if (length > 0.001f) {
        moveDir = moveDir / length;
        m_position = m_position + moveDir * m_flySpeed * deltaTime;
    }

    // Landing sound (when player stops flying and touches ground)
    // For creative mode, we'll just skip landing sounds since always flying
}

void Player::UpdateBlockInteraction(Window* window, World* world, SoundSystem* soundSystem) {
    bool leftClick = window->IsKeyDown(VK_LBUTTON);
    bool rightClick = window->IsKeyDown(VK_RBUTTON);

    // Raycast from camera
    Vector3 rayOrigin = m_camera.GetPosition();
    Vector3 rayDir = m_camera.GetForward();

    Vector3 hitPos, hitNormal;
    Block hitBlock;

    if (world->Raycast(rayOrigin, rayDir, 50.0f, hitPos, hitNormal, hitBlock)) {
        // Play hit sound while holding left click
        if (leftClick && soundSystem) {
            if (!m_leftClickPressed) {
                // First frame of clicking
                soundSystem->PlaySound(SoundSystem::SOUND_BLOCK_HIT);
            }
        }

        // Left click - break block
        if (leftClick && !m_leftClickPressed) {
            world->SetBlock(
                static_cast<int>(hitPos.x),
                static_cast<int>(hitPos.y),
                static_cast<int>(hitPos.z),
                BlockType::Air
            );
            if (soundSystem) {
                soundSystem->PlaySound(SoundSystem::SOUND_BLOCK_BREAK);
            }
        }

        // Right click - place block
        if (rightClick && !m_rightClickPressed) {
            Vector3 placePos = hitPos + hitNormal;
            world->SetBlock(
                static_cast<int>(placePos.x),
                static_cast<int>(placePos.y),
                static_cast<int>(placePos.z),
                GetSelectedBlock()
            );
            if (soundSystem) {
                soundSystem->PlaySound(SoundSystem::SOUND_BLOCK_PLACE);
            }
        }
    }

    m_leftClickPressed = leftClick;
    m_rightClickPressed = rightClick;
}

BlockType Player::GetSelectedBlock() const {
    return m_inventory[m_selectedSlot];
}

void Player::SetSelectedSlot(int slot) {
    if (slot >= 0 && slot < 9) {
        m_selectedSlot = slot;
    }
}
