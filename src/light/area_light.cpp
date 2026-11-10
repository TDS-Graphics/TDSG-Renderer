#include "light/area_light.h"

AreaLight::AreaLight(int primitiveID, const Primitive& primitive, Color radiance)
    : primitiveID(primitiveID), primitive(primitive), radiance(radiance) {}

LightSample AreaLight::sample(Vec3f from, Vec2 u, const ColorSample& color_sample) const {
    float area = primitive.area();
    SurfaceSample surface = primitive.sample(u);
    Vec3f to_light = surface.position - from;
    float distance_squared = lengthSquared(to_light);
    if (!(distance_squared > 0)) {
        return {};
    }

    float distance = std::sqrt(distance_squared);
    Vec3f wi = to_light / distance;
    float cosine = dot(surface.normal, -wi);
    if (cosine <= 0) {
        return {};
    }

    LightSample result;
    result.position = surface.position;
    result.direction = wi;
    result.radiance = color_sample.evaluate(radiance);
    result.pdf = distance_squared / (cosine * area);
    result.distance = distance;
    result.normal = surface.normal;
    return result;
}

Spectrum AreaLight::evaluateEmission(const Hit& hit, Vec3f wo, const ColorSample& color_sample) const {
    if (hit.primitive == primitiveID && dot(hit.geometric_normal, wo) > 0) {
        return color_sample.evaluate(radiance);
    } else {
        return Spectrum(0);
    }
}

float AreaLight::pdf(Vec3f from, const Hit& light_hit) const {
    float area = primitive.area();
    if (light_hit.primitive != primitiveID) {
        return 0;
    }

    Vec3f to_light = light_hit.position - from;
    float distance_squared = lengthSquared(to_light);
    if (!(distance_squared > 0)) {
        return 0;
    }

    float cosine = dot(light_hit.geometric_normal, -to_light / std::sqrt(distance_squared));
    if (cosine <= 0) {
        return 0;
    }

    return distance_squared / (cosine * area);
}

Color AreaLight::power() const {
    return radiance * (Pi * primitive.area());
}
