#pragma once
#include "geometry/primitive.h"

enum class LightType { Point, Area, Spot };

struct LightSample {
    Vec3f position;
    Vec3f direction;
    Spectrum radiance;
    // Solid-angle density for area lights; discrete probability for delta lights.
    float pdf = 0;
    float distance = 0;
    Vec3f normal;
    bool delta = false;
};

class Light {
public:
    virtual ~Light() = default;

    virtual LightType type() const = 0;
    virtual bool isDelta() const = 0;
    virtual LightSample sample(Vec3f from, Vec2 u) const = 0;
    virtual Spectrum evaluateEmission(const Hit& hit, Vec3f wo) const = 0;
    virtual float pdf(Vec3f from, const Hit& light_hit) const = 0;
    virtual Spectrum power() const = 0;

    // Analytic lights have no intersectable primitive.
    virtual int getPrimitiveID() const { return -1; }
};
