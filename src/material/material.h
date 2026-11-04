#pragma once
#include "core/sampler.h"

enum class MaterialType { Diffuse, Metal, Mirror, Glass };

struct BSDFSample {
    Vec3f direction;
    Spectrum weight; // f * abs(dot(n,wi)) / pdf
    float pdf = 0;
    float eta = 1;
    bool delta = false;
};

struct Material {
    MaterialType type = MaterialType::Diffuse;
    Spectrum color = Spectrum(0.75f);
    Spectrum emission = Spectrum(0);
    float roughness = 0.3f;
    float ior = 1.5f;

    bool isDelta() const {
        return (type == MaterialType::Mirror || type == MaterialType::Glass);
    }

    Spectrum eval(Vec3f wo, Vec3f wi, Vec3f n) const;
    float pdf(Vec3f wo, Vec3f wi, Vec3f n) const;
    BSDFSample Sample(Vec3f wo, Vec3f n, bool front_face, Sampler& sampler) const;
};
