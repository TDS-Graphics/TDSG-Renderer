#pragma once
#include "core/ray.h"

struct Bounds {
    Vec3f lower = Vec3f(Infinity), upper = Vec3f(-Infinity);

    void expand(Vec3f p) {
        lower = min(lower, p);
        upper = max(upper, p);
    }

    void expand(const Bounds& b) {
        expand(b.lower);
        expand(b.upper);
    }

    Vec3f center() const { return (lower + upper) * 0.5f; }

    bool Intersect(const Ray& ray, float t_max) const {
        float t_min = ray.t_near;
        t_max = std::min(t_max, ray.t_far);

        for (int axis = 0; axis < 3; ++axis) {
            if (ray.direction[axis] == 0) {
                if (ray.origin[axis] < lower[axis] || ray.origin[axis] > upper[axis]) {
                    return false;
                }

                continue;
            }

            float a = (lower[axis] - ray.origin[axis]) / ray.direction[axis];
            float b = (upper[axis] - ray.origin[axis]) / ray.direction[axis];
            if (a > b) {
                std::swap(a, b);
            }

            t_min = std::max(t_min, a);
            t_max = std::min(t_max, b);
            if (t_min > t_max) {
                return false;
            }
        }

        return true;
    }
};
