#pragma once

// Device-side shading code for the OptiX backend.
//
// This is a deliberate, self-contained copy of the CPU reference implementation
// (src/material/material.cpp and the light classes in src/light). The CPU files
// stay untouched so the two backends can be compared against each other; every
// formula, random-number consumption order and branch here mirrors the CPU code
// one-to-one, which keeps identical seeds reproducible across backends.

#include "mathlib/math.h"
#include "core/sampler.h"
#include "optix/device_scene.h"

#if TDSG_SPECTRAL
#include "color/smits_data.h"
#include "color/spectrum_data.h"
#endif
namespace dev {

// ---------------------------------------------------------------------------
// Spectrum / color arithmetic
// ---------------------------------------------------------------------------

__device__ inline DevSpectrum spectrum(float value) {
    DevSpectrum s;
    for (int i = 0; i < DevSpectrumSize; ++i) {
        s.v[i] = value;
    }
    return s;
}

__device__ inline DevSpectrum operator+(const DevSpectrum& a, const DevSpectrum& b) {
    DevSpectrum r;
    for (int i = 0; i < DevSpectrumSize; ++i) {
        r.v[i] = a.v[i] + b.v[i];
    }
    return r;
}

__device__ inline DevSpectrum operator*(const DevSpectrum& a, const DevSpectrum& b) {
    DevSpectrum r;
    for (int i = 0; i < DevSpectrumSize; ++i) {
        r.v[i] = a.v[i] * b.v[i];
    }
    return r;
}

__device__ inline DevSpectrum operator*(const DevSpectrum& a, float f) {
    DevSpectrum r;
    for (int i = 0; i < DevSpectrumSize; ++i) {
        r.v[i] = a.v[i] * f;
    }
    return r;
}

__device__ inline DevSpectrum& operator+=(DevSpectrum& a, const DevSpectrum& b) {
    for (int i = 0; i < DevSpectrumSize; ++i) {
        a.v[i] += b.v[i];
    }
    return a;
}

__device__ inline DevSpectrum& operator*=(DevSpectrum& a, const DevSpectrum& b) {
    for (int i = 0; i < DevSpectrumSize; ++i) {
        a.v[i] *= b.v[i];
    }
    return a;
}

__device__ inline bool isZero(const DevSpectrum& a) {
    for (int i = 0; i < DevSpectrumSize; ++i) {
        if (a.v[i] != 0) {
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Color sample: one wavelength per path in spectral mode, nothing in RGB mode.
// ---------------------------------------------------------------------------

struct DevColorSample {
#if TDSG_SPECTRAL
    float lambda = 380.0f;
    float pdf = 1.0f;
#endif
};

inline constexpr float LambdaMin = 380.0f;
inline constexpr float LambdaMax = 720.0f;
inline constexpr float LambdaStep = (LambdaMax - LambdaMin) / (DevColorSize - 1);

// Mirrors ColorSample::sample() / SampledWavelength::sample().
__device__ inline DevColorSample sampleColor(Sampler& sampler) {
    DevColorSample cs;
#if TDSG_SPECTRAL
    const float u = sampler.next1D();
    const float range = LambdaMax - LambdaMin;
    cs.lambda = LambdaMin + u * range;
    cs.pdf = 1.0f / range;
#else
    (void)sampler;
#endif
    return cs;
}

// Mirrors ColorSample::evaluate() / DenseSpectrum::evaluate().
__device__ inline DevSpectrum evaluate(const DevColorSample& cs, const DevColor& color) {
    DevSpectrum s;
#if TDSG_SPECTRAL
    if (cs.lambda < LambdaMin || cs.lambda > LambdaMax) {
        s.v[0] = 0;
        return s;
    }
    const float position = (cs.lambda - LambdaMin) / LambdaStep;
    const int i = min(int(position), DevColorSize - 2);
    const float t = position - float(i);
    s.v[0] = (1 - t) * color.v[i] + t * color.v[i + 1];
#else
    for (int i = 0; i < DevSpectrumSize; ++i) {
        s.v[i] = color.v[i];
    }
#endif
    return s;
}

// Mirrors ColorSample::toRGB() / SampledWavelength::toRGB() + xyzToRGB().
// `cie` is the 32x3 CIE table in device memory: constexpr host arrays are not
// readable from device code, so the host uploads them with the scene.
__device__ inline DevVec3 toRGB(const DevColorSample& cs, const DevSpectrum& radiance,
                                const float* cie) {
#if TDSG_SPECTRAL
    const float position = (cs.lambda - LambdaMin) / LambdaStep;
    const int i = min(int(position), DevColorSize - 2);
    const float t = position - float(i);
    float xyz[3];
    for (int c = 0; c < 3; ++c) {
        const float a = cie[i * 3 + c];
        const float b = cie[(i + 1) * 3 + c];
        const float response = (1 - t) * a + t * b;
        xyz[c] = radiance.v[0] * response / cs.pdf;
    }
    return {3.240479f * xyz[0] - 1.537150f * xyz[1] - 0.498535f * xyz[2],
            -0.969256f * xyz[0] + 1.875991f * xyz[1] + 0.041556f * xyz[2],
            0.055648f * xyz[0] - 0.204043f * xyz[1] + 1.057311f * xyz[2]};
#else
    (void)cs;
    (void)cie;
    return {radiance.v[0], radiance.v[1], radiance.v[2]};
#endif
}

#if TDSG_SPECTRAL
// Mirrors DenseSpectrum::fromRGB(rgb, SpectrumType::Illuminant); only needed by
// the normal-visualization integrator. `basis` is the 8x32 Smits illuminant
// table: 8 x DevColorSize floats, row-major.
__device__ inline DevColor fromRGBIlluminant(DevVec3 rgb, const float* basis, const float* cie) {
    DevColor result;
    auto add = [&](int index, float weight) {
        for (int i = 0; i < DevColorSize; ++i) {
            result.v[i] += weight * basis[index * DevColorSize + i];
        }
    };

    const float r = rgb.x, g = rgb.y, b = rgb.z;
    if (r <= g && r <= b) {
        add(0, r);
        if (g <= b) {
            add(1, g - r);
            add(6, b - g);
        } else {
            add(1, b - r);
            add(5, g - b);
        }
    } else if (g <= r && g <= b) {
        add(0, g);
        if (r <= b) {
            add(2, r - g);
            add(6, b - r);
        } else {
            add(2, b - g);
            add(4, r - b);
        }
    } else {
        add(0, b);
        if (r <= g) {
            add(3, r - b);
            add(5, g - r);
        } else {
            add(3, g - b);
            add(4, r - g);
        }
    }

    // White basis luminance, identical to DenseSpectrum::luminance().
    float luminance = 0;
    for (int i = 0; i < DevColorSize - 1; ++i) {
        const float a = cie[i * 3 + 1];
        const float b2 = cie[(i + 1) * 3 + 1];
        luminance += LambdaStep / 6 *
            (basis[i] * (2 * a + b2) + basis[i + 1] * (a + 2 * b2));
    }
    const float scale = 1.0f / luminance;
    for (float& value : result.v) {
        value = fmaxf(0.0f, value * scale);
    }
    return result;
}
#endif

// ---------------------------------------------------------------------------
// BSDF: one-to-one copy of src/material/material.cpp
// ---------------------------------------------------------------------------

__device__ inline float ggxD(float no_h, float alpha) {
    const float a2 = alpha * alpha;
    return a2 / (Pi * square(no_h * no_h * (a2 - 1) + 1));
}

__device__ inline float ggxG1(float no_v, float alpha) {
    return 2 * no_v / (no_v + sqrtf(alpha * alpha + (1 - alpha * alpha) * no_v * no_v));
}

__device__ inline DevSpectrum schlick(const DevSpectrum& f0, float vo_h) {
    const float x = 1 - fminf(fmaxf(vo_h, 0.0f), 1.0f);
    const float t = x * x * x * x * x;  // same value as std::pow(x, 5)
    return f0 + (spectrum(1) + f0 * -1.0f) * t;
}

__device__ inline float dielectricFresnel(float cos_i, float cos_t, float eta) {
    const float rs = (eta * cos_i - cos_t) / (eta * cos_i + cos_t);
    const float rp = (cos_i - eta * cos_t) / (cos_i + eta * cos_t);
    return 0.5f * (rs * rs + rp * rp);
}

__device__ inline bool isDelta(const DeviceMaterial& m) {
    return m.type == DEV_MATERIAL_MIRROR || m.type == DEV_MATERIAL_GLASS;
}

__device__ inline DevSpectrum materialEval(const DeviceMaterial& m, Vec3f wo, Vec3f wi, Vec3f n,
                                           const DevColorSample& cs) {
    const float no_v = dot(n, wo), no_l = dot(n, wi);
    if (no_v <= 0 || no_l <= 0) {
        return spectrum(0);
    }

    if (m.type == DEV_MATERIAL_DIFFUSE) {
        return evaluate(cs, m.color) * (1.0f / Pi);
    }

    if (m.type != DEV_MATERIAL_METAL) {
        return spectrum(0);
    }

    const Vec3f h = normalize(wo + wi);
    const float alpha = fmaxf(0.001f, m.roughness * m.roughness);
    const float d = ggxD(fmaxf(0.0f, dot(n, h)), alpha);
    const float g = ggxG1(no_v, alpha) * ggxG1(no_l, alpha);
    return schlick(evaluate(cs, m.color), dot(wo, h)) * (d * g / (4 * no_v * no_l));
}

__device__ inline float materialPdf(const DeviceMaterial& m, Vec3f wo, Vec3f wi, Vec3f n) {
    const float no_l = dot(n, wi);
    if (dot(n, wo) <= 0 || no_l <= 0) {
        return 0;
    }

    if (m.type == DEV_MATERIAL_DIFFUSE) {
        return no_l / Pi;
    }

    if (m.type != DEV_MATERIAL_METAL) {
        return 0;
    }

    const Vec3f h = normalize(wo + wi);
    const float no_h = fmaxf(0.0f, dot(n, h));
    return ggxD(no_h, fmaxf(0.001f, m.roughness * m.roughness)) * no_h / (4 * dot(wo, h));
}

struct DevBSDFSample {
    Vec3f direction;
    DevSpectrum weight = {};
    float pdf = 0;
    float eta = 1;
    bool delta = false;
};

__device__ inline DevBSDFSample materialSample(const DeviceMaterial& m, Vec3f wo, Vec3f n,
                                               bool front_face, Sampler& sampler,
                                               const DevColorSample& cs) {
    DevBSDFSample result;
    const DevSpectrum reflectance = evaluate(cs, m.color);

    if (m.type == DEV_MATERIAL_MIRROR) {
        result.direction = reflect(wo, n);
        result.weight = reflectance;
        result.pdf = 1;
        result.eta = 1;
        result.delta = true;
        return result;
    }

    if (m.type == DEV_MATERIAL_GLASS) {
        const float eta = front_face ? 1.0f / m.ior : m.ior;
        const float cos_i = fminf(fmaxf(dot(wo, n), 0.0f), 1.0f);
        const float sin2_t = eta * eta * (1 - cos_i * cos_i);
        if (sin2_t >= 1) {
            result.direction = reflect(wo, n);
            result.weight = reflectance;
            result.pdf = 1;
            result.eta = 1;
            result.delta = true;
            return result;
        }

        const float cos_t = sqrtf(1 - sin2_t);
        const float f = dielectricFresnel(cos_i, cos_t, eta);
        if (sampler.next1D() < f) {
            result.direction = reflect(wo, n);
            result.weight = reflectance;
            result.pdf = f;
            result.eta = 1;
            result.delta = true;
            return result;
        }

        result.direction = -eta * wo + (eta * cos_i - cos_t) * n;
        result.weight = reflectance * (eta * eta);
        result.pdf = 1 - f;
        result.eta = 1 / eta;
        result.delta = true;
        return result;
    }

    Vec3f wi;
    const Vec2 u = sampler.next2D();
    if (m.type == DEV_MATERIAL_DIFFUSE) {
        wi = Frame(n).toWorld(cosineHemisphere(u));
    } else {
        const float alpha = fmaxf(0.001f, m.roughness * m.roughness);
        const float cos_theta = sqrtf((1 - u.x) / (1 + (alpha * alpha - 1) * u.x));
        const float sin_theta = sqrtf(fmaxf(0.0f, 1 - cos_theta * cos_theta));
        const float phi = 2 * Pi * u.y;
        const Vec3f h = Frame(n).toWorld({sin_theta * cosf(phi), sin_theta * sinf(phi), cos_theta});
        if (dot(wo, h) <= 0) {
            return result;
        }

        wi = reflect(wo, h);
    }

    const float p = materialPdf(m, wo, wi, n);
    if (p == 0) {
        return result;
    }

    result.direction = wi;
    result.weight = materialEval(m, wo, wi, n, cs) * (dot(n, wi) / p);
    result.pdf = p;
    return result;
}

// ---------------------------------------------------------------------------
// Lights: one-to-one copy of src/light/*.cpp and Lights::sampleLight/pdf/emission
// ---------------------------------------------------------------------------

struct DevLightSample {
    Vec3f position;
    Vec3f direction;
    DevSpectrum radiance = {};
    float pdf = 0;
    float distance = 0;
    Vec3f normal;
    bool delta = false;
};

// Mirrors Primitive::sample().
__device__ inline void sampleSurface(const DeviceSceneData& scene, int32_t primitive, Vec2 u,
                                     Vec3f& position, Vec3f& normal) {
    if (primitive < int32_t(scene.triangleCount)) {
        const DevVec3* p = scene.triVertices + size_t(primitive) * 3;
        const Vec3f p0{p[0].x, p[0].y, p[0].z};
        const Vec3f p1{p[1].x, p[1].y, p[1].z};
        const Vec3f p2{p[2].x, p[2].y, p[2].z};
        const float r = sqrtf(u.x);
        position = p0 * (1 - r) + p1 * (r * (1 - u.y)) + p2 * (r * u.y);
        normal = normalize(cross(p1 - p0, p2 - p0));
    } else {
        const DeviceSphere& s = scene.spheres[primitive - int32_t(scene.triangleCount)];
        const Vec3f n = uniformSphere(u);
        position = Vec3f{s.center.x, s.center.y, s.center.z} + s.radius * n;
        normal = n;
    }
}

__device__ inline float spotFalloff(const DeviceLight& light, Vec3f from_light) {
    const float cosine = dot(Vec3f{light.axis.x, light.axis.y, light.axis.z}, from_light);
    if (cosine < light.cosOuter) {
        return 0;
    }
    if (cosine >= light.cosInner) {
        return 1;
    }

    const float t = (cosine - light.cosOuter) / (light.cosInner - light.cosOuter);
    return square(square(t));
}

__device__ inline DevLightSample lightSample(const DeviceSceneData& scene, const DeviceLight& light,
                                             Vec3f from, Vec2 u, const DevColorSample& cs) {
    DevLightSample result;

    if (light.type == DEV_LIGHT_AREA) {
        Vec3f position, normal;
        sampleSurface(scene, light.primitive, u, position, normal);

        const Vec3f to_light = position - from;
        const float distance_squared = lengthSquared(to_light);
        if (!(distance_squared > 0)) {
            return result;
        }

        const float distance = sqrtf(distance_squared);
        const Vec3f wi = to_light / distance;
        const float cosine = dot(normal, -wi);
        if (cosine <= 0) {
            return result;
        }

        result.position = position;
        result.direction = wi;
        result.radiance = evaluate(cs, light.areaRadiance);
        result.pdf = distance_squared / (cosine * light.area);
        result.distance = distance;
        result.normal = normal;
        return result;
    }

    const Vec3f position{light.position.x, light.position.y, light.position.z};
    const Vec3f to_light = position - from;
    const float distance_squared = lengthSquared(to_light);
    if (!(distance_squared > 0)) {
        return result;
    }

    const DevSpectrum intensity = evaluate(cs, light.intensity);

    result.position = position;
    result.distance = sqrtf(distance_squared);
    result.direction = to_light / result.distance;
    if (light.type == DEV_LIGHT_SPOT) {
        result.radiance = intensity * (spotFalloff(light, -result.direction) / distance_squared);
    } else {
        result.radiance = intensity * (1.0f / distance_squared);
    }

    result.pdf = 1;
    result.delta = true;
    return result;
}

// Mirrors Lights::sampleLight().
__device__ inline DevLightSample sampleLight(const DeviceSceneData& scene, Vec3f from,
                                             Sampler& sampler, const DevColorSample& cs) {
    if (!(scene.lightWeight > 0) || scene.lightCount == 0) {
        return {};
    }

    const double target = double(sampler.next1D()) * scene.lightWeight;
    // std::upper_bound over cumulative weights.
    uint32_t lo = 0, hi = scene.lightCount;
    while (lo < hi) {
        const uint32_t mid = (lo + hi) / 2;
        if (target < scene.lights[mid].cumulative) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }
    const uint32_t index = lo < scene.lightCount ? lo : scene.lightCount - 1;

    DevLightSample result = lightSample(scene, scene.lights[index], from, sampler.next2D(), cs);
    result.pdf *= scene.lights[index].probability;
    return result;
}

// Mirrors AreaLight::pdf(); delta lights return 0 in the CPU code as well.
__device__ inline float lightPdf(const DeviceSceneData& scene, const DeviceLight& light, Vec3f from,
                                 int32_t primitive, Vec3f geometric_normal, Vec3f position) {
    if (light.type != DEV_LIGHT_AREA || primitive != light.primitive) {
        return 0;
    }

    const Vec3f to_light = position - from;
    const float distance_squared = lengthSquared(to_light);
    if (!(distance_squared > 0)) {
        return 0;
    }

    const float cosine = dot(geometric_normal, -to_light / sqrtf(distance_squared));
    if (cosine <= 0) {
        return 0;
    }

    return distance_squared / (cosine * light.area);
}

// Mirrors Lights::emission().
__device__ inline DevSpectrum lightEmission(const DeviceSceneData& scene, int32_t lightIndex,
                                            int32_t primitive, Vec3f geometric_normal, Vec3f wo,
                                            const DevColorSample& cs) {
    if (lightIndex < 0 || lightIndex >= int32_t(scene.lightCount)) {
        return spectrum(0);
    }

    const DeviceLight& light = scene.lights[lightIndex];
    if (light.type != DEV_LIGHT_AREA || primitive != light.primitive) {
        return spectrum(0);
    }

    if (!(dot(geometric_normal, wo) > 0)) {
        return spectrum(0);
    }

    return evaluate(cs, light.areaRadiance);
}

}  // namespace dev
