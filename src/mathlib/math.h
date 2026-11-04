#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

constexpr float Pi = 3.14159265358979323846f;
constexpr float Infinity = std::numeric_limits<float>::infinity();
constexpr float EPS = 1e-4;

struct Vec2 { float x = 0, y = 0; };

inline Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }

struct Vec3f {
    float x = 0, y = 0, z = 0;

    Vec3f() = default;
    Vec3f(float v) : x(v), y(v), z(v) {}
    Vec3f(float x, float y, float z) : x(x), y(y), z(z) {}

    float operator[](int i) const {
        if (i == 0) {
            return x;
        } else if (i == 1) {
            return y;
        } else {
            return z;
        }
    }
};

using RGBSpectrum = Vec3f;
using Spectrum = RGBSpectrum;

inline Vec3f operator+(Vec3f a, Vec3f b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3f operator-(Vec3f a, Vec3f b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3f operator-(Vec3f a) { return {-a.x, -a.y, -a.z}; }
inline Vec3f operator*(Vec3f a, Vec3f b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
inline Vec3f operator*(Vec3f a, float b) { return {a.x * b, a.y * b, a.z * b}; }
inline Vec3f operator*(float a, Vec3f b) { return b * a; }
inline Vec3f operator/(Vec3f a, float b) { return a * (1.0f / b); }
inline Vec3f& operator+=(Vec3f& a, Vec3f b) {
    a = a + b;
    return a;
}

inline Vec3f& operator*=(Vec3f& a, Vec3f b) {
    a = a * b;
    return a;
}

inline Vec3f& operator*=(Vec3f& a, float b) {
    a = a * b;
    return a;
}

inline float dot(Vec3f a, Vec3f b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

inline Vec3f cross(Vec3f a, Vec3f b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

inline float lengthSquared(Vec3f v) { return dot(v, v); }
inline float length(Vec3f v) { return std::sqrt(lengthSquared(v)); }
inline Vec3f normalize(Vec3f v) { return v / length(v); }
inline Vec3f min(Vec3f a, Vec3f b) { return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)}; }
inline Vec3f max(Vec3f a, Vec3f b) { return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)}; }
inline float maxComponent(Vec3f v) { return std::max({v.x, v.y, v.z}); }
inline float luminance(Spectrum c) { return dot(c, {0.2126f, 0.7152f, 0.0722f}); }
inline float square(float x) { return x * x; }
inline float radians(float degrees) { return degrees * Pi / 180.0f; }
inline Vec3f reflect(Vec3f wo, Vec3f n) { return 2.0f * dot(wo, n) * n - wo; }
inline float powerHeuristic(float a, float b) { return a * a / (a * a + b * b); }

struct Frame {
    Vec3f x, y, z;

    explicit Frame(Vec3f n) : z(n) {
        Vec3f reference;
        if (std::abs(n.z) < 0.999f) {
            reference = {0, 0, 1};
        } else {
            reference = {0, 1, 0};
        }

        x = normalize(cross(reference, n));
        y = cross(z, x);
    }

    Vec3f toWorld(Vec3f v) const { return x * v.x + y * v.y + z * v.z; }
};

inline Vec3f cosineHemisphere(Vec2 u) {
    float r = std::sqrt(u.x), phi = 2 * Pi * u.y;
    return {r * std::cos(phi), r * std::sin(phi), std::sqrt(1 - u.x)};
}

inline Vec3f uniformSphere(Vec2 u) {
    float z = 1 - 2 * u.x, r = std::sqrt(std::max(0.0f, 1 - z * z));
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
