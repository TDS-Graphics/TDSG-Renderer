#include "film/film.h"
#include <cmath>
#include <cstdint>
#include <fstream>
#include <stdexcept>

unsigned char displayByte(float linear) {
    linear = std::max(0.0f, linear);
    float mapped = linear / (1 + linear);
    float srgb;
    if (mapped <= 0.0031308f) {
        srgb = 12.92f * mapped;
    } else {
        srgb = 1.055f * std::pow(mapped, 1 / 2.4f) - 0.055f;
    }

    return static_cast<unsigned char>(255 * std::clamp(srgb, 0.0f, 1.0f) + 0.5f);
}

void Film::addSample(int x, int y, const SpectrumRGB& color) {
    size_t i = size_t(y) * image_width + x;
    sums[i] += color;
    ++counts[i];
}

void Film::addSamples(int x, int y, const SpectrumRGB& color, int count) {
    if (count <= 0) {
        return;
    }
    size_t i = size_t(y) * image_width + x;
    sums[i] += color * float(count);
    counts[i] += count;
}

SpectrumRGB Film::pixel(int x, int y) const {
    size_t i = size_t(y) * image_width + x;
    if (counts[i]) {
        return sums[i] / float(counts[i]);
    } else {
        return SpectrumRGB(0);
    }
}

void Film::Save(const std::filesystem::path& path, float exposure) const {
    bool hdr = path.extension() == ".pfm";
    if (!hdr && path.extension() != ".ppm") {
        throw std::runtime_error("Output extension must be .ppm or .pfm");
    }

    std::ofstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot write image: " + path.string());
    }

    if (hdr) {
        uint16_t endian_test = 1;
        bool little_endian = *reinterpret_cast<unsigned char*>(&endian_test) == 1;
        file << "PF\n" << image_width << ' ' << image_height;
        if (little_endian) {
            file << "\n-1.0\n";
        } else {
            file << "\n1.0\n";
        }

        for (int y = image_height - 1; y >= 0; --y) {
            for (int x = 0; x < image_width; ++x) {
                SpectrumRGB c = pixel(x, y);
                float channels[] = {c[0], c[1], c[2]};
                file.write(reinterpret_cast<const char*>(channels), sizeof(channels));
            }
        }
    } else {
        file << "P6\n" << image_width << ' ' << image_height << "\n255\n";
        float scale = std::exp2(exposure);

        for (int y = 0; y < image_height; ++y) {
            for (int x = 0; x < image_width; ++x) {
                SpectrumRGB c = pixel(x, y) * scale;
                unsigned char channels[] = {displayByte(c[0]), displayByte(c[1]), displayByte(c[2])};
                file.write(reinterpret_cast<const char*>(channels), sizeof(channels));
            }
        }
    }

    if (!file) {
        throw std::runtime_error("Image write failed: " + path.string());
    }
}
