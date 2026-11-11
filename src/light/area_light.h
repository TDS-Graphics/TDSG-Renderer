#pragma once
#include "light/light.h"

class AreaLight : public Light {
public:
    AreaLight(int primitiveID, const Primitive& primitive, Color radiance);

    LightType type() const override { return LightType::Area; }
    bool isDelta() const override { return false; }
    LightSample sample(Vec3f from, Vec2 u, const ColorSample& color_sample) const override;
    Spectrum evaluateEmission(const Hit& hit, Vec3f wo, const ColorSample& color_sample) const override;
    float pdf(Vec3f from, const Hit& light_hit) const override;
    Color power() const override;
    int getPrimitiveID() const override { return primitiveID; }

    // Read-only accessor used by the OptiX backend to flatten the scene.
    Color getRadiance() const { return radiance; }

private:
    int primitiveID;
    // A value snapshot keeps this light independent of Scene storage and lifetime.
    Primitive primitive;
    Color radiance;
};
