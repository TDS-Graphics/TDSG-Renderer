#pragma once
#include "color/rgb.h"
#include <initializer_list>

enum class SpectrumType {
    Reflectance, Illuminant
};
struct SpectralSample {
    float wavelength, value;
};

class DenseSpectrum {
public:
    static constexpr int SampleCount = 32;
    static constexpr float LambdaMin = 380;
    static constexpr float LambdaMax = 720;
    static constexpr float LambdaStep = (LambdaMax - LambdaMin) / (SampleCount - 1);

    DenseSpectrum() = default;
    explicit DenseSpectrum(float value) { values.fill(value); }

    static constexpr float wavelength(int i) { return LambdaMin + i * LambdaStep; }

    // Linear sRGB: reflectance inputs in [0, 1], illuminants nonnegative (HDR allowed).
    static DenseSpectrum fromRGB(const SpectrumRGB& rgb, SpectrumType type = SpectrumType::Reflectance);

    static DenseSpectrum fromSamples(std::initializer_list<SpectralSample> samples);

    float operator[](int i) const { return values[i]; }
    float& operator[](int i) { return values[i]; }
    float evaluate(float lambda) const;

    DenseSpectrum operator+(const DenseSpectrum& rhs) const {
        DenseSpectrum result;
        for (int i = 0; i < SampleCount; ++i) result[i] = values[i] + rhs[i];
        return result;
    }
    DenseSpectrum operator-(const DenseSpectrum& rhs) const {
        DenseSpectrum result;
        for (int i = 0; i < SampleCount; ++i) result[i] = values[i] - rhs[i];
        return result;
    }
    DenseSpectrum operator*(const DenseSpectrum& rhs) const {
        DenseSpectrum result;
        for (int i = 0; i < SampleCount; ++i) result[i] = values[i] * rhs[i];
        return result;
    }
    DenseSpectrum operator/(const DenseSpectrum& rhs) const {
        DenseSpectrum result;
        for (int i = 0; i < SampleCount; ++i) result[i] = values[i] / rhs[i];
        return result;
    }
    DenseSpectrum operator*(float f) const {
        DenseSpectrum result;
        for (int i = 0; i < SampleCount; ++i) result[i] = values[i] * f;
        return result;
    }
    DenseSpectrum operator/(float f) const { return *this * (1 / f); }

    DenseSpectrum& operator+=(const DenseSpectrum& rhs) { return *this = *this + rhs; }
    DenseSpectrum& operator-=(const DenseSpectrum& rhs) { return *this = *this - rhs; }
    DenseSpectrum& operator*=(const DenseSpectrum& rhs) { return *this = *this * rhs; }
    DenseSpectrum& operator/=(const DenseSpectrum& rhs) { return *this = *this / rhs; }
    DenseSpectrum& operator*=(float f) { return *this = *this * f; }
    DenseSpectrum& operator/=(float f) { return *this = *this / f; }

    bool isZero() const {
        return std::all_of(values.begin(), values.end(), [](float v) { return v == 0; });
    }
    float maxComponent() const { return *std::max_element(values.begin(), values.end()); }
    std::array<float, 3> toXYZ() const;
    SpectrumRGB toRGB() const { return xyzToRGB(toXYZ()); }
    float luminance() const;

private:
    std::array<float, SampleCount> values{};
};

inline DenseSpectrum operator*(float f, const DenseSpectrum& s) { return s * f; }
inline float maxComponent(const DenseSpectrum& s) { return s.maxComponent(); }
inline float luminance(const DenseSpectrum& s) { return s.luminance(); }

// One uniformly sampled wavelength per camera path. PDF is measured in 1/nm.
struct SampledWavelength {
    float lambda;
    float pdf;

    // u is uniform in [0, 1).
    static SampledWavelength sample(float u) {
        float range = DenseSpectrum::LambdaMax - DenseSpectrum::LambdaMin;
        return {DenseSpectrum::LambdaMin + u * range, 1 / range};
    }
    SpectrumRGB toRGB(float radiance) const;
};
