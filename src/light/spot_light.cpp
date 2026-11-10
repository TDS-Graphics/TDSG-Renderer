#include "light/spot_light.h"

SpotLight::SpotLight(Vec3f position, Vec3f direction, Color intensity,
                     float inner_angle, float outer_angle)
    : position(position), direction(normalize(direction)), intensity(intensity) {
    cos_inner = std::cos(radians(inner_angle));
    cos_outer = std::cos(radians(outer_angle));
}

float SpotLight::falloff(Vec3f from_light) const {
    float cosine = dot(direction, from_light);
    if (cosine < cos_outer) {
        return 0;
    }
    if (cosine >= cos_inner) {
        return 1;
    }

    float t = (cosine - cos_outer) / (cos_inner - cos_outer);
    return square(square(t));
}

LightSample SpotLight::sample(Vec3f from, Vec2, const ColorSample& color_sample) const {
    Vec3f to_light = position - from;
    float distance_squared = lengthSquared(to_light);
    if (!(distance_squared > 0)) {
        return {};
    }

    LightSample result;
    result.position = position;
    result.distance = std::sqrt(distance_squared);
    result.direction = to_light / result.distance;
    result.radiance = color_sample.evaluate(intensity) * (falloff(-result.direction) / distance_squared);
    result.pdf = 1;
    result.delta = true;
    return result;
}

Color SpotLight::power() const {
    // Integral of the constant inner cone plus the quartic falloff ring.
    float solid_angle = 2 * Pi * ((1 - cos_inner) + (cos_inner - cos_outer) / 5);
    return intensity * solid_angle;
}
