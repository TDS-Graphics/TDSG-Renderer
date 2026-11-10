#pragma once
#include <algorithm>
#include <array>

// 线性 sRGB
class SpectrumRGB {
public:
    SpectrumRGB() = default;
    SpectrumRGB(float value) : rgb{value, value, value} {}
    SpectrumRGB(float r, float g, float b) : rgb{r, g, b} {}

    float operator[](int i) const { return rgb[i]; }
    float& operator[](int i) { return rgb[i]; }

    SpectrumRGB operator+(const SpectrumRGB& rhs) const {
        return {rgb[0] + rhs[0], rgb[1] + rhs[1], rgb[2] + rhs[2]};
    }
    SpectrumRGB operator-(const SpectrumRGB& rhs) const {
        return {rgb[0] - rhs[0], rgb[1] - rhs[1], rgb[2] - rhs[2]};
    }
    SpectrumRGB operator*(const SpectrumRGB& rhs) const {
        return {rgb[0] * rhs[0], rgb[1] * rhs[1], rgb[2] * rhs[2]};
    }
    SpectrumRGB operator/(const SpectrumRGB& rhs) const {
        return {rgb[0] / rhs[0], rgb[1] / rhs[1], rgb[2] / rhs[2]};
    }
    SpectrumRGB operator*(float f) const { return {rgb[0] * f, rgb[1] * f, rgb[2] * f}; }
    SpectrumRGB operator/(float f) const { return *this * (1 / f); }

    SpectrumRGB& operator+=(const SpectrumRGB& rhs) { return *this = *this + rhs; }
    SpectrumRGB& operator-=(const SpectrumRGB& rhs) { return *this = *this - rhs; }
    SpectrumRGB& operator*=(const SpectrumRGB& rhs) { return *this = *this * rhs; }
    SpectrumRGB& operator/=(const SpectrumRGB& rhs) { return *this = *this / rhs; }
    SpectrumRGB& operator*=(float f) { return *this = *this * f; }
    SpectrumRGB& operator/=(float f) { return *this = *this / f; }

    bool isZero() const { return rgb[0] == 0 && rgb[1] == 0 && rgb[2] == 0; }

private:
    std::array<float, 3> rgb{};
};

inline SpectrumRGB operator*(float f, const SpectrumRGB& rgb) { return rgb * f; }
inline float maxComponent(const SpectrumRGB& rgb) { return std::max({rgb[0], rgb[1], rgb[2]}); }
inline float luminance(const SpectrumRGB& rgb) {
    return 0.2126f * rgb[0] + 0.7152f * rgb[1] + 0.0722f * rgb[2];
}

inline SpectrumRGB xyzToRGB(const std::array<float, 3>& xyz) {
    return {3.240479f * xyz[0] - 1.537150f * xyz[1] - 0.498535f * xyz[2],
           -0.969256f * xyz[0] + 1.875991f * xyz[1] + 0.041556f * xyz[2],
            0.055648f * xyz[0] - 0.204043f * xyz[1] + 1.057311f * xyz[2]};
}

inline std::array<float, 3> rgbToXYZ(const SpectrumRGB& rgb) {
    return {0.412453f * rgb[0] + 0.357580f * rgb[1] + 0.180423f * rgb[2],
            0.212671f * rgb[0] + 0.715160f * rgb[1] + 0.072169f * rgb[2],
            0.019334f * rgb[0] + 0.119193f * rgb[1] + 0.950227f * rgb[2]};
}
