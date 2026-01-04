#include "Camera.h"
#include <cmath>

const float PI = 3.14159265359f;

Camera::Camera()
    : m_position(0, 0, 0)
    , m_targetPosition(0, 0, 0)
    , m_yaw(0)
    , m_pitch(0)
    , m_aspectRatio(16.0f / 9.0f)
    , m_fov(70.0f * PI / 180.0f)
    , m_nearPlane(0.1f)
    , m_farPlane(1000.0f)
    , m_mode(CameraMode::FirstPerson) {
}

void Camera::SetPosition(const Vector3& pos) {
    m_position = pos;
}

void Camera::SetRotation(float yaw, float pitch) {
    m_yaw = yaw;
    m_pitch = pitch;

    // Clamp pitch
    if (m_pitch > 89.0f * PI / 180.0f) m_pitch = 89.0f * PI / 180.0f;
    if (m_pitch < -89.0f * PI / 180.0f) m_pitch = -89.0f * PI / 180.0f;
}

void Camera::SetAspectRatio(float aspect) {
    m_aspectRatio = aspect;
}

Vector3 Camera::GetForward() const {
    Vector3 forward;
    forward.x = std::cos(m_pitch) * std::sin(m_yaw);
    forward.y = std::sin(m_pitch);
    forward.z = std::cos(m_pitch) * std::cos(m_yaw);
    return forward.normalized();
}

Vector3 Camera::GetRight() const {
    Vector3 forward = GetForward();
    Vector3 worldUp(0, 1, 0);
    // Use worldUp x forward for left-handed coordinates.
    return worldUp.cross(forward).normalized();
}

Vector3 Camera::GetUp() const {
    Vector3 right = GetRight();
    Vector3 forward = GetForward();
    return forward.cross(right).normalized();
}

Matrix4x4 Camera::GetViewMatrix() const {
    Vector3 eye = m_position;
    Vector3 forward = GetForward();
    Vector3 up = GetUp();

    // Adjust camera position based on mode
    if (m_mode == CameraMode::ThirdPersonBack) {
        eye = m_targetPosition - forward * 5.0f + Vector3(0, 1.5f, 0);
    } else if (m_mode == CameraMode::ThirdPersonFront) {
        eye = m_targetPosition + forward * 5.0f + Vector3(0, 1.5f, 0);
    }

    Vector3 at = eye + forward;
    return Matrix4x4::LookAtLH(eye, at, up);
}

Matrix4x4 Camera::GetProjectionMatrix() const {
    return Matrix4x4::PerspectiveFovLH(m_fov, m_aspectRatio, m_nearPlane, m_farPlane);
}

void Camera::ToggleMode() {
    switch (m_mode) {
    case CameraMode::FirstPerson:
        m_mode = CameraMode::ThirdPersonBack;
        break;
    case CameraMode::ThirdPersonBack:
        m_mode = CameraMode::ThirdPersonFront;
        break;
    case CameraMode::ThirdPersonFront:
        m_mode = CameraMode::FirstPerson;
        break;
    }
}
