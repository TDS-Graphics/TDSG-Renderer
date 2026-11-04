#include "material/material.h"

float ggxD(float no_h, float alpha) {
    float a2 = alpha * alpha;
    return a2 / (Pi * square(no_h * no_h * (a2 - 1) + 1));
}

float ggxG1(float no_v, float alpha) {
    return 2 * no_v / (no_v + std::sqrt(alpha * alpha + (1 - alpha * alpha) * no_v * no_v));
}

Spectrum schlick(Spectrum f0, float vo_h) {
    float t = std::pow(1 - std::clamp(vo_h, 0.0f, 1.0f), 5.0f);
    return f0 + (Spectrum(1) - f0) * t;
}

float dielectricFresnel(float cos_i, float cos_t, float eta) {
    float rs = (eta * cos_i - cos_t) / (eta * cos_i + cos_t);
    float rp = (cos_i - eta * cos_t) / (cos_i + eta * cos_t);
    return 0.5f * (rs * rs + rp * rp);
}

Spectrum Material::eval(Vec3f wo, Vec3f wi, Vec3f n) const {
    float no_v = dot(n, wo), no_l = dot(n, wi);
    if (no_v <= 0 || no_l <= 0) {
        return Spectrum(0);
    }

    if (type == MaterialType::Diffuse) {
        return color / Pi;
    }

    if (type != MaterialType::Metal) {
        return Spectrum(0);
    }

    Vec3f h = normalize(wo + wi);
    float alpha = std::max(0.001f, roughness * roughness);
    float d = ggxD(std::max(0.0f, dot(n, h)), alpha);
    float g = ggxG1(no_v, alpha) * ggxG1(no_l, alpha);
    return schlick(color, dot(wo, h)) * (d * g / (4 * no_v * no_l));
}

float Material::pdf(Vec3f wo, Vec3f wi, Vec3f n) const {
    float no_l = dot(n, wi);
    if (dot(n, wo) <= 0 || no_l <= 0) {
        return 0;
    }

    if (type == MaterialType::Diffuse) {
        return no_l / Pi;
    }

    if (type != MaterialType::Metal) {
        return 0;
    }

    Vec3f h = normalize(wo + wi);
    float no_h = std::max(0.0f, dot(n, h));
    return ggxD(no_h, std::max(0.001f, roughness * roughness)) * no_h / (4 * dot(wo, h));
}

BSDFSample Material::Sample(Vec3f wo, Vec3f n, bool front_face, Sampler& sampler) const {
    if (type == MaterialType::Mirror) {
        return {reflect(wo, n), color, 1, 1, true};
    }

    if (type == MaterialType::Glass) {
        float eta;
        if (front_face) {
            eta = 1 / ior;
        } else {
            eta = ior;
        }

        float cos_i = std::clamp(dot(wo, n), 0.0f, 1.0f);
        float sin2_t = eta * eta * (1 - cos_i * cos_i);
        if (sin2_t >= 1) {
            return {reflect(wo, n), color, 1, 1, true};
        }

        float cos_t = std::sqrt(1 - sin2_t);
        float f = dielectricFresnel(cos_i, cos_t, eta);
        if (sampler.next1D() < f) {
            return {reflect(wo, n), color, f, 1, true};
        }

        Vec3f wi = -eta * wo + (eta * cos_i - cos_t) * n;
        return {wi, color * (eta * eta), 1 - f, 1 / eta, true};
    }

    Vec3f wi;
    Vec2 u = sampler.next2D();
    if (type == MaterialType::Diffuse) {
        wi = Frame(n).toWorld(cosineHemisphere(u));
    } else {
        float alpha = std::max(0.001f, roughness * roughness);
        float cos_theta = std::sqrt((1 - u.x) / (1 + (alpha * alpha - 1) * u.x));
        float sin_theta = std::sqrt(std::max(0.0f, 1 - cos_theta * cos_theta));
        float phi = 2 * Pi * u.y;
        Vec3f h = Frame(n).toWorld({sin_theta * std::cos(phi), sin_theta * std::sin(phi), cos_theta});
        if (dot(wo, h) <= 0) {
            return {};
        }

        wi = reflect(wo, h);
    }

    float p = pdf(wo, wi, n);
    if (p == 0) {
        return {};
    }

    return {wi, eval(wo, wi, n) * (dot(n, wi) / p), p};
}
