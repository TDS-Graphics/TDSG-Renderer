#pragma once
#include <array>
#include <variant>
#include "core/bounds.h"

struct Hit {
    Vec3f position;
    Vec3f geometric_normal;
    Vec3f normal;
    float t = Infinity;
    int materialID = -1;
    int primitive = -1;
    bool front_face = true;
};

struct SurfaceSample {
    Vec3f position;
    Vec3f normal;
};

struct Sphere {
    Vec3f center;
    float radius;
};

struct Triangle {
    std::array<Vec3f, 3> positions;
    std::array<Vec3f, 3> normals{};
    bool smooth = false;
};

struct Primitive {
    std::variant<Sphere, Triangle> shape;
    int materialID;

    Bounds getBounds() const;
    float area() const;
    bool Intersect(const Ray& ray, Hit& hit) const;
    SurfaceSample sample(Vec2 u) const;
};
