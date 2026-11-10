#pragma once
#include "color/color.h"
#include "core/sampler.h"

enum class MaterialType { Diffuse, Metal, Mirror, Glass };

float dielectricFresnel(float cos_i, float cos_t, float eta);

struct BSDFSample {
    Vec3f direction;
    Spectrum weight = Spectrum(0); // f * abs(dot(n,wi)) / pdf
    float pdf = 0;
    float eta = 1;
    bool delta = false;
};

struct Material {
    MaterialType type = MaterialType::Diffuse;
    Color color = Color(0.75f);
    Color emission = Color(0);
    float roughness = 0.3f;
    float ior = 1.5f;

    bool isDelta() const {
        return (type == MaterialType::Mirror || type == MaterialType::Glass);
    }

    Spectrum eval(Vec3f wo, Vec3f wi, Vec3f n, const ColorSample& color_sample) const;
    float pdf(Vec3f wo, Vec3f wi, Vec3f n) const;
    BSDFSample Sample(Vec3f wo, Vec3f n, bool front_face, Sampler& sampler, const ColorSample& color_sample) const;
};
