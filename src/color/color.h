#pragma once
#include "color/spectrum.h"

// Color stores scene inputs; Spectrum carries radiance and BSDF weights along a path.
#if TDSG_SPECTRAL
using Color = DenseSpectrum;
using Spectrum = float;

struct ColorSample {
    SampledWavelength wavelength;

    // Only next1D() is required, so color/ does not depend on the renderer's Sampler.
    template<class Sampler>
    static ColorSample sample(Sampler& sampler) {
        return {SampledWavelength::sample(sampler.next1D())};
    }

    Spectrum evaluate(const Color& color) const { return color.evaluate(wavelength.lambda); }
    SpectrumRGB toRGB(Spectrum radiance) const { return wavelength.toRGB(radiance); }
};

inline Color fromRGB(const SpectrumRGB& rgb, SpectrumType type = SpectrumType::Reflectance) {
    return DenseSpectrum::fromRGB(rgb, type);
}
inline Color fromSamples(std::initializer_list<SpectralSample> samples) {
    return DenseSpectrum::fromSamples(samples);
}
inline SpectrumRGB toRGB(const Color& color) { return color.toRGB(); }
inline bool isZero(Spectrum value) { return value == 0; }

#else
using Color = SpectrumRGB;
using Spectrum = SpectrumRGB;

struct ColorSample {
    template<class Sampler>
    static ColorSample sample(Sampler&) { return {}; }

    Spectrum evaluate(const Color& color) const { return color; }
    SpectrumRGB toRGB(const Spectrum& radiance) const { return radiance; }
};

inline Color fromRGB(const SpectrumRGB& rgb, SpectrumType = SpectrumType::Reflectance) {
    return rgb;
}
inline Color fromSamples(std::initializer_list<SpectralSample> samples) {
    return DenseSpectrum::fromSamples(samples).toRGB();
}
inline SpectrumRGB toRGB(const Color& color) { return color; }
inline bool isZero(const Spectrum& value) { return value.isZero(); }
#endif
