#pragma once
#include "MathUtils.h"

enum class CameraMode {
    FirstPerson,
    ThirdPersonBack,
    ThirdPersonFront
};

class Camera {
public:
    Camera();

    void SetPosition(const Vector3& pos);
    void SetRotation(float yaw, float pitch);
    void SetAspectRatio(float aspect);

    Vector3 GetPosition() const { return m_position; }
    Vector3 GetForward() const;
    Vector3 GetRight() const;
    Vector3 GetUp() const;

    Matrix4x4 GetViewMatrix() const;
    Matrix4x4 GetProjectionMatrix() const;

    void ToggleMode();
    CameraMode GetMode() const { return m_mode; }

    void SetTargetPosition(const Vector3& target) { m_targetPosition = target; }

    float GetYaw() const { return m_yaw; }
    float GetPitch() const { return m_pitch; }

private:
    Vector3 m_position;
    Vector3 m_targetPosition;
    float m_yaw;
    float m_pitch;
    float m_aspectRatio;
    float m_fov;
    float m_nearPlane;
    float m_farPlane;
    CameraMode m_mode;

    void UpdateCameraPosition();
};
