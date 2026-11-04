#include "geometry/primitive.h"

Bounds Primitive::getBounds() const {
    Bounds b;
    if (const auto* s = std::get_if<Sphere>(&shape)) {
        b.expand(s->center - Vec3f(s->radius));
        b.expand(s->center + Vec3f(s->radius));
    } else {
        for (Vec3f p : std::get<Triangle>(shape).positions) {
            b.expand(p);
        }
    }

    return b;
}

float Primitive::area() const {
    if (const auto* s = std::get_if<Sphere>(&shape)) {
        return 4 * Pi * s->radius * s->radius;
    }

    const auto& p = std::get<Triangle>(shape).positions;
    return 0.5f * length(cross(p[1] - p[0], p[2] - p[0]));
}

bool Primitive::Intersect(const Ray& ray, Hit& hit) const {
    float t;
    Vec3f ng, ns;

    if (const auto* s = std::get_if<Sphere>(&shape)) {
        Vec3f oc = ray.origin - s->center;
        float a = dot(ray.direction, ray.direction), half_b = dot(oc, ray.direction);
        float c = dot(oc, oc) - s->radius * s->radius;
        float discriminant = half_b * half_b - a * c;
        if (discriminant < 0) {
            return false;
        }

        float root = std::sqrt(discriminant);
        t = (-half_b - root) / a;
        if (t <= ray.t_near) {
            t = (-half_b + root) / a;
        }

        if (t <= ray.t_near || t >= std::min(ray.t_far, hit.t)) {
            return false;
        }

        ng = normalize(ray.at(t) - s->center);
        ns = ng;
    } else {
        // Möller–Trumbore 快速三角形求交
        const auto& tri = std::get<Triangle>(shape);
        const auto& p = tri.positions;
        Vec3f e1 = p[1] - p[0], e2 = p[2] - p[0];
        Vec3f q = cross(ray.direction, e2);
        float det = dot(e1, q);
        if (det == 0) {
            return false;
        }

        Vec3f offset = ray.origin - p[0];
        float u = dot(offset, q) / det;
        if (u < 0 || u > 1) {
            return false;
        }

        Vec3f r = cross(offset, e1);
        float v = dot(ray.direction, r) / det;
        if (v < 0 || u + v > 1) {
            return false;
        }

        t = dot(e2, r) / det;
        if (t <= ray.t_near || t >= std::min(ray.t_far, hit.t)) {
            return false;
        }

        ng = normalize(cross(e1, e2));
        if (tri.smooth) {
            ns = normalize(tri.normals[0] * (1 - u - v) + tri.normals[1] * u + tri.normals[2] * v);
        } else {
            ns = ng;
        }

        if (dot(ns, ng) < 0) {
            ns = -ns;
        }
    }

    hit.t = t;
    hit.position = ray.at(t);
    hit.geometric_normal = ng;
    hit.front_face = dot(ray.direction, ng) < 0;
    if (hit.front_face) {
        hit.normal = ns;
    } else {
        hit.normal = -ns;
    }
    hit.materialID = materialID;
    return true;
}

SurfaceSample Primitive::sample(Vec2 u) const {
    if (const auto* s = std::get_if<Sphere>(&shape)) {
        Vec3f n = uniformSphere(u);
        return {s->center + s->radius * n, n};
    }

    const auto& p = std::get<Triangle>(shape).positions;
    float r = std::sqrt(u.x);
    return {p[0] * (1 - r) + p[1] * (r * (1 - u.y)) + p[2] * (r * u.y),
            normalize(cross(p[1] - p[0], p[2] - p[0]))};
}
