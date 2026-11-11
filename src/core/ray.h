#pragma once
#include "mathlib/math.h"

struct Ray {
    Ray() = default;

    Ray(Vec3f origin, Vec3f direction, float t_near = 1e-4f,
        float t_far = 1e10f, float time = 0)
        : origin(origin), direction(normalize(direction)),
          t_near(t_near), t_far(t_far), time(time) {}

    TDSG_HD Vec3f at(float distance) const { return origin + distance * direction; }

    Vec3f origin{}, direction{};
    float t_near = 1e-4f;
    float t_far = 1e10f;
    float time = 0;

    bool has_differentials = false;
    Vec3f origin_x{}, origin_y{};
    Vec3f direction_x{}, direction_y{};
};

TDSG_HD inline float rayEpsilon(Vec3f p) {
    const float ax = p.x < 0 ? -p.x : p.x;
    const float ay = p.y < 0 ? -p.y : p.y;
    const float az = p.z < 0 ? -p.z : p.z;
    float largest = ax > ay ? ax : ay;
    largest = largest > az ? largest : az;
    largest = largest > 1.0f ? largest : 1.0f;
    return 1e-5f * largest;
}

TDSG_HD inline Vec3f offsetOrigin(Vec3f p, Vec3f n, Vec3f direction) {
    float epsilon = rayEpsilon(p);
    if (dot(n, direction) >= 0) {
        return p + n * epsilon;
    } else {
        return p + n * -epsilon;
    }
}
