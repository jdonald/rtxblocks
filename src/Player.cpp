#include "Player.h"
#include "Window.h"
#include "World.h"
#include "SoundSystem.h"
#include <algorithm>
#include <vector>

const float PI = 3.14159265359f;

Player::Player()
    : m_position(0, 100, 0)
    , m_velocity(0, 0, 0)
    , m_verticalVelocity(0.0f)
    , m_yaw(0)
    , m_pitch(0)
    , m_isFlying(false)
    , m_onGround(false)
    , m_moveSpeed(10.0f)
    , m_flySpeed(20.0f)
    , m_mouseSensitivity(0.002f)
    , m_gravity(-25.0f)
    , m_jumpVelocity(8.0f)
    , m_spaceTapTimer(999.0f)
    , m_doubleTapWindow(0.25f)
    , m_selectedSlot(0)
    , m_leftClickPressed(false)
    , m_rightClickPressed(false)
    , m_wasOnGround(false)
    , m_fallDistance(0.0f)
    , m_indexCount(0) {

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

    // Handle double-tap to toggle flying
    m_spaceTapTimer += deltaTime;
    if (window->WasKeyPressed(VK_SPACE)) {
        if (m_spaceTapTimer <= m_doubleTapWindow) {
            m_isFlying = !m_isFlying;
            m_verticalVelocity = 0.0f;
            m_fallDistance = 0.0f;
        }
        m_spaceTapTimer = 0.0f;
    }

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

    if (m_isFlying) {
        if (window->IsKeyDown(VK_SPACE)) {
            moveDir = moveDir + up;
        }
        if (window->IsKeyDown(VK_SHIFT)) {
            moveDir = moveDir - up;
        }
    }

    // Normalize and apply speed
    float length = moveDir.length();
    if (length > 0.001f) {
        moveDir = moveDir / length;
        float speed = m_isFlying ? m_flySpeed : m_moveSpeed;
        m_position = m_position + moveDir * speed * deltaTime;
    }

    // Gravity/jumping when not flying
    if (!m_isFlying) {
        m_verticalVelocity += m_gravity * deltaTime;
        m_position.y += m_verticalVelocity * deltaTime;

        // Ground check via raycast
        const float playerHeight = 1.7f;
        Vector3 hitPos, hitNormal;
        Block hitBlock;
        bool hitGround = world && world->Raycast(m_position, Vector3(0, -1, 0), playerHeight + 0.3f, hitPos, hitNormal, hitBlock);

        if (hitGround) {
            float groundY = hitPos.y + 1.0f + playerHeight;
            if (m_position.y <= groundY) {
                m_position.y = groundY;
                m_verticalVelocity = 0.0f;
                m_onGround = true;
            } else {
                m_onGround = false;
            }
        } else {
            m_onGround = false;
        }

        if (m_onGround && window->WasKeyPressed(VK_SPACE)) {
            m_verticalVelocity = m_jumpVelocity;
            m_onGround = false;
        }

        if (!m_onGround && m_verticalVelocity < 0.0f) {
            m_fallDistance += -m_verticalVelocity * deltaTime;
        }

        if (m_onGround && !m_wasOnGround) {
            if (m_fallDistance > 1.0f && soundSystem) {
                soundSystem->PlaySound(SoundSystem::SOUND_LAND);
            }
            m_fallDistance = 0.0f;
        }

        m_wasOnGround = m_onGround;
    } else {
        m_onGround = false;
        m_wasOnGround = false;
        m_fallDistance = 0.0f;
    }
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
            BlockType brokenType = hitBlock.type;
            world->SetBlock(
                static_cast<int>(hitPos.x),
                static_cast<int>(hitPos.y),
                static_cast<int>(hitPos.z),
                BlockType::Air
            );
            if (soundSystem) {
                soundSystem->PlaySound(SoundSystem::SOUND_BLOCK_BREAK);
            }
            AddBlockToInventory(brokenType);
        }

        // Right click - place block
        if (rightClick && !m_rightClickPressed) {
            BlockType selected = GetSelectedBlock();
            if (selected != BlockType::Air) {
                Vector3 placePos = hitPos + hitNormal;
                world->SetBlock(
                    static_cast<int>(placePos.x),
                    static_cast<int>(placePos.y),
                    static_cast<int>(placePos.z),
                    selected
                );
                if (soundSystem) {
                    soundSystem->PlaySound(SoundSystem::SOUND_BLOCK_PLACE);
                }
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

void Player::AddBlockToInventory(BlockType type) {
    if (type == BlockType::Air) {
        return;
    }

    for (int i = 0; i < 9; i++) {
        if (m_inventory[i] == BlockType::Air) {
            m_inventory[i] = type;
            return;
        }
    }

    m_inventory[m_selectedSlot] = type;
}

void Player::CreateMesh(ID3D11Device* device) {
    if (!device) return;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    Vector4 skinColor(0.95f, 0.80f, 0.65f, 1.0f);
    Vector4 shirtColor(0.2f, 0.6f, 0.9f, 1.0f);
    Vector4 pantsColor(0.1f, 0.2f, 0.6f, 1.0f);
    Vector4 hairColor(0.15f, 0.1f, 0.05f, 1.0f);
    Vector4 shoeColor(0.1f, 0.1f, 0.1f, 1.0f);

    auto addBox = [&](const Vector3& center, const Vector3& size, const Vector4& color) {
        Vector3 min = center - size * 0.5f;
        Vector3 max = center + size * 0.5f;

        uint32_t baseIdx = static_cast<uint32_t>(vertices.size());

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

        struct Face {
            int indices[4];
            Vector3 normal;
        };

        Face faces[6] = {
            {{0, 1, 2, 3}, Vector3(0, 0, -1)},
            {{5, 4, 7, 6}, Vector3(0, 0, 1)},
            {{4, 0, 3, 7}, Vector3(-1, 0, 0)},
            {{1, 5, 6, 2}, Vector3(1, 0, 0)},
            {{3, 2, 6, 7}, Vector3(0, 1, 0)},
            {{4, 5, 1, 0}, Vector3(0, -1, 0)}
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

    // Head
    addBox(Vector3(0.0f, 1.7f, 0.0f), Vector3(0.6f, 0.6f, 0.6f), skinColor);
    addBox(Vector3(0.0f, 1.85f, 0.0f), Vector3(0.62f, 0.3f, 0.62f), hairColor);

    // Body
    addBox(Vector3(0.0f, 1.0f, 0.0f), Vector3(0.8f, 0.9f, 0.4f), shirtColor);

    // Arms
    addBox(Vector3(-0.65f, 1.05f, 0.0f), Vector3(0.25f, 0.9f, 0.25f), skinColor);
    addBox(Vector3(0.65f, 1.05f, 0.0f), Vector3(0.25f, 0.9f, 0.25f), skinColor);

    // Legs
    addBox(Vector3(-0.25f, 0.35f, 0.0f), Vector3(0.3f, 0.7f, 0.3f), pantsColor);
    addBox(Vector3(0.25f, 0.35f, 0.0f), Vector3(0.3f, 0.7f, 0.3f), pantsColor);
    addBox(Vector3(-0.25f, 0.05f, 0.0f), Vector3(0.32f, 0.1f, 0.32f), shoeColor);
    addBox(Vector3(0.25f, 0.05f, 0.0f), Vector3(0.32f, 0.1f, 0.32f), shoeColor);

    if (vertices.empty()) return;

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(Vertex));
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = vertices.data();

    device->CreateBuffer(&vbDesc, &vbData, m_vertexBuffer.ReleaseAndGetAddressOf());

    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.ByteWidth = static_cast<UINT>(indices.size() * sizeof(uint32_t));
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = indices.data();

    device->CreateBuffer(&ibDesc, &ibData, m_indexBuffer.ReleaseAndGetAddressOf());

    m_indexCount = static_cast<uint32_t>(indices.size());
}

void Player::Render(ID3D11DeviceContext* context) {
    if (!m_vertexBuffer || !m_indexBuffer || m_indexCount == 0) return;

    UINT stride = sizeof(Vertex);
    UINT offset = 0;

    context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
    context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    context->DrawIndexed(m_indexCount, 0, 0);
}

Matrix4x4 Player::GetWorldMatrix() const {
    float eyeHeight = 1.7f;
    Matrix4x4 rotation = Matrix4x4::RotationY(m_yaw);
    Matrix4x4 translation = Matrix4x4::Translation(m_position.x, m_position.y - eyeHeight, m_position.z);
    return rotation * translation;
}
