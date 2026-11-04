#pragma once
#include "light/light.h"

class SpotLight : public Light {
public:
    // Direction points out of the light. Cone angles are half-angles in degrees.
    SpotLight(Vec3f position, Vec3f direction, Spectrum intensity,
              float inner_angle = 20, float outer_angle = 30);

    LightType type() const override { return LightType::Spot; }
    bool isDelta() const override { return true; }
    LightSample sample(Vec3f from, Vec2 u) const override;
    Spectrum evaluateEmission(const Hit&, Vec3f) const override { return Spectrum(0); }
    float pdf(Vec3f, const Hit&) const override { return 0; }
    Spectrum power() const override;

private:
    float falloff(Vec3f from_light) const;

    Vec3f position;
    Vec3f direction;
    Spectrum intensity;
    float cos_inner, cos_outer;
};
