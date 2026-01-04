#pragma once
#include <cmath>
#include <algorithm>

struct Vector3 {
    float x, y, z;

    Vector3() : x(0), y(0), z(0) {}
    Vector3(float x, float y, float z) : x(x), y(y), z(z) {}

    Vector3 operator+(const Vector3& other) const {
        return Vector3(x + other.x, y + other.y, z + other.z);
    }

    Vector3 operator-(const Vector3& other) const {
        return Vector3(x - other.x, y - other.y, z - other.z);
    }

    Vector3 operator*(float scalar) const {
        return Vector3(x * scalar, y * scalar, z * scalar);
    }

    Vector3 operator/(float scalar) const {
        return Vector3(x / scalar, y / scalar, z / scalar);
    }

    float dot(const Vector3& other) const {
        return x * other.x + y * other.y + z * other.z;
    }

    Vector3 cross(const Vector3& other) const {
        return Vector3(
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        );
    }

    float length() const {
        return std::sqrt(x * x + y * y + z * z);
    }

    Vector3 normalized() const {
        float len = length();
        if (len > 0.0001f) {
            return *this / len;
        }
        return Vector3(0, 0, 0);
    }
};

struct Vector2 {
    float x, y;

    Vector2() : x(0), y(0) {}
    Vector2(float x, float y) : x(x), y(y) {}
};

struct Vector4 {
    float x, y, z, w;

    Vector4() : x(0), y(0), z(0), w(0) {}
    Vector4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    Vector4(const Vector3& v, float w) : x(v.x), y(v.y), z(v.z), w(w) {}
};

struct Matrix4x4 {
    float m[4][4];

    Matrix4x4() {
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                m[i][j] = (i == j) ? 1.0f : 0.0f;
            }
        }
    }

    static Matrix4x4 Identity() {
        return Matrix4x4();
    }

    static Matrix4x4 Translation(float x, float y, float z) {
        Matrix4x4 result;
        result.m[0][3] = x;
        result.m[1][3] = y;
        result.m[2][3] = z;
        return result;
    }

    static Matrix4x4 RotationX(float angle) {
        Matrix4x4 result;
        float c = std::cos(angle);
        float s = std::sin(angle);
        result.m[1][1] = c;
        result.m[1][2] = -s;
        result.m[2][1] = s;
        result.m[2][2] = c;
        return result;
    }

    static Matrix4x4 RotationY(float angle) {
        Matrix4x4 result;
        float c = std::cos(angle);
        float s = std::sin(angle);
        result.m[0][0] = c;
        result.m[0][2] = s;
        result.m[2][0] = -s;
        result.m[2][2] = c;
        return result;
    }

    static Matrix4x4 RotationZ(float angle) {
        Matrix4x4 result;
        float c = std::cos(angle);
        float s = std::sin(angle);
        result.m[0][0] = c;
        result.m[0][1] = -s;
        result.m[1][0] = s;
        result.m[1][1] = c;
        return result;
    }

    static Matrix4x4 PerspectiveFovLH(float fov, float aspect, float nearZ, float farZ) {
        Matrix4x4 result;
        float yScale = 1.0f / std::tan(fov * 0.5f);
        float xScale = yScale / aspect;

        result.m[0][0] = xScale;
        result.m[1][1] = yScale;
        result.m[2][2] = farZ / (farZ - nearZ);
        result.m[2][3] = -nearZ * farZ / (farZ - nearZ);
        result.m[3][2] = 1.0f;
        result.m[3][3] = 0.0f;

        return result;
    }

    static Matrix4x4 LookAtLH(const Vector3& eye, const Vector3& at, const Vector3& up) {
        Vector3 zAxis = (at - eye).normalized();
        Vector3 xAxis = up.cross(zAxis).normalized();
        Vector3 yAxis = zAxis.cross(xAxis);

        Matrix4x4 result;
        result.m[0][0] = xAxis.x;
        result.m[0][1] = yAxis.x;
        result.m[0][2] = zAxis.x;
        result.m[0][3] = 0.0f;

        result.m[1][0] = xAxis.y;
        result.m[1][1] = yAxis.y;
        result.m[1][2] = zAxis.y;
        result.m[1][3] = 0.0f;

        result.m[2][0] = xAxis.z;
        result.m[2][1] = yAxis.z;
        result.m[2][2] = zAxis.z;
        result.m[2][3] = 0.0f;

        result.m[3][0] = -xAxis.dot(eye);
        result.m[3][1] = -yAxis.dot(eye);
        result.m[3][2] = -zAxis.dot(eye);
        result.m[3][3] = 1.0f;

        return result;
    }

    Matrix4x4 operator*(const Matrix4x4& other) const {
        Matrix4x4 result;
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                result.m[i][j] = 0;
                for (int k = 0; k < 4; k++) {
                    result.m[i][j] += m[i][k] * other.m[k][j];
                }
            }
        }
        return result;
    }

    Matrix4x4 Transpose() const {
        Matrix4x4 result;
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                result.m[i][j] = m[j][i];
            }
        }
        return result;
    }
};

inline float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

inline float Clamp(float value, float min, float max) {
    return std::max(min, std::min(max, value));
}
