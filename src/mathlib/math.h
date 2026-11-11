#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

// Shared math primitives. Annotated so the OptiX device programs can reuse them
// verbatim (the macro is empty for the C++ build).
#ifndef TDSG_HD
#if defined(__CUDACC__)
#define TDSG_HD __host__ __device__
#else
#define TDSG_HD
#endif
#endif

constexpr float Pi = 3.14159265358979323846f;
constexpr float Infinity = std::numeric_limits<float>::infinity();
constexpr float EPS = 1e-4;

struct Vec2 { float x = 0, y = 0; };

TDSG_HD inline Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }

struct Vec3f {
    float x = 0, y = 0, z = 0;

    TDSG_HD Vec3f() = default;
    TDSG_HD Vec3f(float v) : x(v), y(v), z(v) {}
    TDSG_HD Vec3f(float x, float y, float z) : x(x), y(y), z(z) {}

    TDSG_HD float operator[](int i) const {
        if (i == 0) {
            return x;
        } else if (i == 1) {
            return y;
        } else {
            return z;
        }
    }
};

TDSG_HD inline Vec3f operator+(Vec3f a, Vec3f b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
TDSG_HD inline Vec3f operator-(Vec3f a, Vec3f b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
TDSG_HD inline Vec3f operator-(Vec3f a) { return {-a.x, -a.y, -a.z}; }
TDSG_HD inline Vec3f operator*(Vec3f a, Vec3f b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
TDSG_HD inline Vec3f operator*(Vec3f a, float b) { return {a.x * b, a.y * b, a.z * b}; }
TDSG_HD inline Vec3f operator*(float a, Vec3f b) { return b * a; }
TDSG_HD inline Vec3f operator/(Vec3f a, float b) { return a * (1.0f / b); }
TDSG_HD inline Vec3f& operator+=(Vec3f& a, Vec3f b) {
    a = a + b;
    return a;
}

TDSG_HD inline Vec3f& operator*=(Vec3f& a, Vec3f b) {
    a = a * b;
    return a;
}

TDSG_HD inline Vec3f& operator*=(Vec3f& a, float b) {
    a = a * b;
    return a;
}

TDSG_HD inline float dot(Vec3f a, Vec3f b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

TDSG_HD inline Vec3f cross(Vec3f a, Vec3f b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

TDSG_HD inline float lengthSquared(Vec3f v) { return dot(v, v); }
TDSG_HD inline float length(Vec3f v) { return std::sqrt(lengthSquared(v)); }
TDSG_HD inline Vec3f normalize(Vec3f v) { return v / length(v); }
TDSG_HD inline Vec3f min(Vec3f a, Vec3f b) { return {a.x < b.x ? a.x : b.x, a.y < b.y ? a.y : b.y, a.z < b.z ? a.z : b.z}; }
TDSG_HD inline Vec3f max(Vec3f a, Vec3f b) { return {a.x > b.x ? a.x : b.x, a.y > b.y ? a.y : b.y, a.z > b.z ? a.z : b.z}; }
TDSG_HD inline float maxComponent(Vec3f v) { return v.x > v.y ? (v.x > v.z ? v.x : v.z) : (v.y > v.z ? v.y : v.z); }
TDSG_HD inline float square(float x) { return x * x; }
TDSG_HD inline float radians(float degrees) { return degrees * Pi / 180.0f; }
TDSG_HD inline Vec3f reflect(Vec3f wo, Vec3f n) { return 2.0f * dot(wo, n) * n - wo; }
TDSG_HD inline float powerHeuristic(float a, float b) { return a * a / (a * a + b * b); }

struct Frame {
    Vec3f x, y, z;

    TDSG_HD explicit Frame(Vec3f n) : z(n) {
        Vec3f reference;
        const float abs_z = n.z < 0 ? -n.z : n.z;
        if (abs_z < 0.999f) {
            reference = {0, 0, 1};
        } else {
            reference = {0, 1, 0};
        }

        x = normalize(cross(reference, n));
        y = cross(z, x);
    }

    TDSG_HD Vec3f toWorld(Vec3f v) const { return x * v.x + y * v.y + z * v.z; }
};

TDSG_HD inline Vec3f cosineHemisphere(Vec2 u) {
    float r = std::sqrt(u.x), phi = 2 * Pi * u.y;
    return {r * std::cos(phi), r * std::sin(phi), std::sqrt(1 - u.x)};
}

TDSG_HD inline Vec3f uniformSphere(Vec2 u) {
    float z = 1 - 2 * u.x, r = std::sqrt(0.0f > 1 - z * z ? 0.0f : 1 - z * z);
    float phi = 2 * Pi * u.y;
    return {r * std::cos(phi), r * std::sin(phi), z};
}


struct Transform {
    Vec3f translation;
    float scale = 1, rotation_y = 0;

    Vec3f rotate(Vec3f p) const {
        float c = std::cos(radians(rotation_y)), s = std::sin(radians(rotation_y));
        return {c * p.x + s * p.z, p.y, -s * p.x + c * p.z};
    }

    Vec3f point(Vec3f p) const { return rotate(p * scale) + translation; }
    Vec3f normal(Vec3f n) const { return normalize(rotate(n)); }
};
