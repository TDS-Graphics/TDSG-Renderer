#pragma once
#include <filesystem>
#include <vector>
#include "color/rgb.h"

class Film {
public:
    Film(int width, int height) : image_width(width), image_height(height), sums(size_t(width) * height), counts(sums.size()) {}

    int width() const { return image_width; }
    int height() const { return image_height; }

    void addSample(int x, int y, const SpectrumRGB& color);
    // Adds `count` identical samples at once; used by the GPU backend, which
    // accumulates a whole pixel before copying it back.
    void addSamples(int x, int y, const SpectrumRGB& color, int count);
    SpectrumRGB pixel(int x, int y) const;
    void Save(const std::filesystem::path& path, float exposure = 0) const;

private:
    int image_width, image_height;
    std::vector<SpectrumRGB> sums;
    std::vector<int> counts;
};
