// render/Math.h - the small vector toolkit the CPU viewport needs.
//
// The scene stores Vec3 as plain data; the renderer adds the arithmetic on top so the
// data layer stays free of maths and the maths stays header-only.
#pragma once

#include <cmath>

#include "astra/scene/Scene.h"

namespace astra::render {

using scene::Vec3;

inline Vec3 operator+(const Vec3& a, const Vec3& b) { return Vec3(a.x + b.x, a.y + b.y, a.z + b.z); }
inline Vec3 operator-(const Vec3& a, const Vec3& b) { return Vec3(a.x - b.x, a.y - b.y, a.z - b.z); }
inline Vec3 operator-(const Vec3& a) { return Vec3(-a.x, -a.y, -a.z); }
inline Vec3 operator*(const Vec3& a, float s) { return Vec3(a.x * s, a.y * s, a.z * s); }
inline Vec3 operator*(float s, const Vec3& a) { return a * s; }
inline Vec3 operator*(const Vec3& a, const Vec3& b) {
    return Vec3(a.x * b.x, a.y * b.y, a.z * b.z);
}
inline Vec3& operator+=(Vec3& a, const Vec3& b) {
    a.x += b.x;
    a.y += b.y;
    a.z += b.z;
    return a;
}

inline float Dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

inline Vec3 Cross(const Vec3& a, const Vec3& b) {
    return Vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

inline float Length(const Vec3& a) { return std::sqrt(Dot(a, a)); }

inline Vec3 Normalize(const Vec3& a) {
    const float length = Length(a);
    return length > 1e-6f ? a * (1.0f / length) : Vec3(0.0f, 1.0f, 0.0f);
}

inline Vec3 Lerp(const Vec3& a, const Vec3& b, float t) { return a + (b - a) * t; }

inline float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

inline float Radians(float degrees) { return degrees * 0.01745329251994329577f; }

// Rotates a vector by intrinsic X, then Y, then Z (the order the inspector shows).
inline Vec3 RotateEuler(const Vec3& v, const Vec3& degrees) {
    const float rx = Radians(degrees.x);
    const float ry = Radians(degrees.y);
    const float rz = Radians(degrees.z);

    Vec3 p = v;
    // X
    {
        const float c = std::cos(rx);
        const float s = std::sin(rx);
        p = Vec3(p.x, p.y * c - p.z * s, p.y * s + p.z * c);
    }
    // Y
    {
        const float c = std::cos(ry);
        const float s = std::sin(ry);
        p = Vec3(p.x * c + p.z * s, p.y, -p.x * s + p.z * c);
    }
    // Z
    {
        const float c = std::cos(rz);
        const float s = std::sin(rz);
        p = Vec3(p.x * c - p.y * s, p.x * s + p.y * c, p.z);
    }
    return p;
}

inline bool Finite(const Vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

}  // namespace astra::render
