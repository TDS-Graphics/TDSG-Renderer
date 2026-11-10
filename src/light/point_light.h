#pragma once
#include "light/light.h"

class PointLight : public Light {
public:
    // Intensity is radiant intensity (W/sr), not emitted surface radiance.
    PointLight(Vec3f position, Color intensity);

    LightType type() const override { return LightType::Point; }
    bool isDelta() const override { return true; }
    LightSample sample(Vec3f from, Vec2 u, const ColorSample& color_sample) const override;
    Spectrum evaluateEmission(const Hit&, Vec3f, const ColorSample&) const override { return Spectrum(0); }
    float pdf(Vec3f, const Hit&) const override { return 0; }
    Color power() const override;

private:
    Vec3f position;
    Color intensity;
};
