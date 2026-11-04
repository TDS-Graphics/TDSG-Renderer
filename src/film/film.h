#pragma once
#include <filesystem>
#include <vector>
#include "mathlib/math.h"

class Film {
public:
    Film(int width, int height) : image_width(width), image_height(height), sums(size_t(width) * height), counts(sums.size()) {}

    int width() const { return image_width; }
    int height() const { return image_height; }

    void addSample(int x, int y, Spectrum color);
    Spectrum pixel(int x, int y) const;
    void Save(const std::filesystem::path& path, float exposure = 0) const;

private:
    int image_width, image_height;
    std::vector<Spectrum> sums;
    std::vector<int> counts;
};
