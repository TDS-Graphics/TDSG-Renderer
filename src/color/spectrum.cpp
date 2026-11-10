#include "color/spectrum.h"
#include "color/spectrum_data.h"
#include "color/smits_data.h"

DenseSpectrum DenseSpectrum::fromRGB(const SpectrumRGB& rgb, SpectrumType type) {
    const float (*basis)[SampleCount];
    if (type == SpectrumType::Reflectance) {
        basis = color_detail::RGBReflectance;
    } else {
        basis = color_detail::RGBIlluminant;
    }

    DenseSpectrum result;
    auto add = [&](int index, float weight) {
        for (int i = 0; i < SampleCount; ++i) {
            result[i] += weight * basis[index][i];
        }
    };

    // Smits decomposition: white + one secondary + one primary color.
    float r = rgb[0], g = rgb[1], b = rgb[2];
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

    // Normalize the illuminant white basis to Y = 1 on our wavelength grid.
    static const float illuminant_scale = [] {
        DenseSpectrum white;
        for (int i = 0; i < SampleCount; ++i) {
            white[i] = color_detail::RGBIlluminant[0][i];
        }
        return 1 / white.luminance();
    }();
    for (float& value : result.values) {
        if (type == SpectrumType::Reflectance) {
            value = std::clamp(value * 0.94f, 0.0f, 1.0f);
        } else {
            value = std::max(0.0f, value * illuminant_scale);
        }
    }
    return result;
}

DenseSpectrum DenseSpectrum::fromSamples(std::initializer_list<SpectralSample> samples) {
    DenseSpectrum result;
    auto left = samples.begin();
    for (int i = 0; i < SampleCount; ++i) {
        float lambda = wavelength(i);
        if (lambda < samples.begin()->wavelength || lambda > (samples.end() - 1)->wavelength) continue;
        while (left + 1 < samples.end() - 1 && (left + 1)->wavelength < lambda) ++left;
        auto right = left + 1;
        float t = (lambda - left->wavelength) / (right->wavelength - left->wavelength);
        result[i] = (1 - t) * left->value + t * right->value;
    }
    return result;
}

float DenseSpectrum::evaluate(float lambda) const {
    if (lambda < LambdaMin || lambda > LambdaMax) {
        return 0;
    }
    float position = (lambda - LambdaMin) / LambdaStep;
    int i = std::min(int(position), SampleCount - 2);
    float t = position - i;
    return (1 - t) * values[i] + t * values[i + 1];
}

std::array<float, 3> DenseSpectrum::toXYZ() const {
    std::array<float, 3> xyz{};
    // Exact integral of two piecewise linear functions: spectrum and CIE response.
    for (int i = 0; i < SampleCount - 1; ++i) {
        for (int channel = 0; channel < 3; ++channel) {
            float a = color_detail::CIE[i][channel];
            float b = color_detail::CIE[i + 1][channel];
            xyz[channel] += LambdaStep / 6 *
                (values[i] * (2 * a + b) + values[i + 1] * (a + 2 * b));
        }
    }
    return xyz;
}

float DenseSpectrum::luminance() const {
    return toXYZ()[1];
}

SpectrumRGB SampledWavelength::toRGB(float radiance) const {
    float position = (lambda - DenseSpectrum::LambdaMin) / DenseSpectrum::LambdaStep;
    int i = std::min(int(position), DenseSpectrum::SampleCount - 2);
    float t = position - i;
    std::array<float, 3> xyz{};
    for (int c = 0; c < 3; ++c) {
        float response = (1 - t) * color_detail::CIE[i][c] + t * color_detail::CIE[i + 1][c];
        xyz[c] = radiance * response / pdf;
    }
    return xyzToRGB(xyz);
}
