#pragma once
// Minimal vec3 / mat4 math. No external dependency on purpose — keeps the
// build to "a C++ compiler + GL headers", nothing else to fetch.

#include <cmath>
#include <cstring>

namespace ft {

struct Vec3 {
    float x = 0, y = 0, z = 0;

    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator-() const { return {-x, -y, -z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }

    float lengthSq() const { return x * x + y * y + z * z; }
    float length() const { return std::sqrt(lengthSq()); }

    Vec3 normalized() const {
        float l = length();
        if (l < 1e-6f) return {0, 0, 0};
        return {x / l, y / l, z / l};
    }
};

inline float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

inline Vec3 lerp(const Vec3& a, const Vec3& b, float t) { return a + (b - a) * t; }

// Column-major 4x4, matching GL's expected memory layout.
struct Mat4 {
    float m[16] = {1, 0, 0, 0,
                    0, 1, 0, 0,
                    0, 0, 1, 0,
                    0, 0, 0, 1};

    static Mat4 identity() { return Mat4(); }

    static Mat4 translation(const Vec3& t) {
        Mat4 r;
        r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z;
        return r;
    }

    static Mat4 scale(float s) {
        Mat4 r;
        r.m[0] = s; r.m[5] = s; r.m[10] = s;
        return r;
    }

    // Builds a basis matrix from a forward direction and a world-up hint,
    // used to orient a fish mesh (authored facing +X) along its velocity.
    static Mat4 basisFromForward(const Vec3& forward, const Vec3& worldUp) {
        Vec3 f = forward.normalized();
        if (f.lengthSq() < 1e-8f) f = Vec3(1, 0, 0);
        Vec3 right = cross(f, worldUp).normalized();
        if (right.lengthSq() < 1e-8f) right = Vec3(0, 0, 1);
        Vec3 up = cross(right, f).normalized();
        Mat4 r;
        // Columns: right(+X authoring axis), up(+Y), -forward(+Z, right-handed), translation.
        r.m[0] = right.x; r.m[1] = right.y; r.m[2] = right.z; r.m[3] = 0;
        r.m[4] = up.x;    r.m[5] = up.y;    r.m[6] = up.z;    r.m[7] = 0;
        r.m[8] = -f.x;    r.m[9] = -f.y;    r.m[10] = -f.z;   r.m[11] = 0;
        r.m[12] = 0;      r.m[13] = 0;      r.m[14] = 0;      r.m[15] = 1;
        return r;
    }

    static Mat4 multiply(const Mat4& a, const Mat4& b) {
        Mat4 r;
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                float sum = 0;
                for (int k = 0; k < 4; ++k) sum += a.m[k * 4 + row] * b.m[col * 4 + k];
                r.m[col * 4 + row] = sum;
            }
        }
        return r;
    }

    static Mat4 perspective(float fovYRadians, float aspect, float nearZ, float farZ) {
        Mat4 r;
        std::memset(r.m, 0, sizeof(r.m));
        float f = 1.0f / std::tan(fovYRadians * 0.5f);
        r.m[0] = f / aspect;
        r.m[5] = f;
        r.m[10] = (farZ + nearZ) / (nearZ - farZ);
        r.m[11] = -1.0f;
        r.m[14] = (2.0f * farZ * nearZ) / (nearZ - farZ);
        return r;
    }

    static Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
        Vec3 f = (center - eye).normalized();
        Vec3 s = cross(f, up).normalized();
        Vec3 u = cross(s, f);
        Mat4 r;
        r.m[0] = s.x; r.m[4] = s.y; r.m[8] = s.z;
        r.m[1] = u.x; r.m[5] = u.y; r.m[9] = u.z;
        r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
        r.m[12] = -dot(s, eye);
        r.m[13] = -dot(u, eye);
        r.m[14] = dot(f, eye);
        r.m[15] = 1.0f;
        return r;
    }
};

inline Mat4 operator*(const Mat4& a, const Mat4& b) { return Mat4::multiply(a, b); }

} // namespace ft
