#include "light/point_light.h"

PointLight::PointLight(Vec3f position, Spectrum intensity)
    : position(position), intensity(intensity) {}

LightSample PointLight::sample(Vec3f from, Vec2) const {
    Vec3f to_light = position - from;
    float distance_squared = lengthSquared(to_light);
    if (!(distance_squared > 0)) {
        return {};
    }

    LightSample result;
    result.position = position;
    result.distance = std::sqrt(distance_squared);
    result.direction = to_light / result.distance;
    result.radiance = intensity / distance_squared;
    result.pdf = 1;
    result.delta = true;
    return result;
}

Spectrum PointLight::power() const {
    return intensity * (4 * Pi);
}
