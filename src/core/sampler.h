#pragma once
#include <cstdint>
#include "mathlib/math.h"

class Sampler {
public:
    TDSG_HD Sampler(uint64_t seed = 1, uint64_t sequence = 1)
        : state(0), increment((sequence << 1u) | 1u) {
        nextU32();
        state += seed;
        nextU32();
    }

    TDSG_HD static Sampler forSample(uint64_t seed, uint64_t pixel, uint64_t sample) {
        uint64_t stream = mix(pixel) ^ mix(sample + 0x9e3779b97f4a7c15ULL);

        return Sampler(mix(seed ^ stream), stream);
    }

    TDSG_HD float next1D() {
        return float(nextU32() >> 8) * (1.0f / 16777216.0f);
    }

    TDSG_HD Vec2 next2D() {
        float x = next1D();
        return {x, next1D()};
    }

private:
    uint64_t state;
    uint64_t increment;

    // PCG-XSH-RR 64/32.
    // 来自 https://www.pcg-random.org/
    TDSG_HD uint32_t nextU32() {
        uint64_t old_state = state;
        state = old_state * 6364136223846793005ULL + increment;

        uint32_t xorshifted = uint32_t(((old_state >> 18u) ^ old_state) >> 27u);
        uint32_t rotation = uint32_t(old_state >> 59u);
        return (xorshifted >> rotation) | (xorshifted << ((32u - rotation) & 31u));
    }

    TDSG_HD static uint64_t mix(uint64_t v) {
        v = (v ^ (v >> 30)) * 0xbf58476d1ce4e5b9ULL;
        v = (v ^ (v >> 27)) * 0x94d049bb133111ebULL;
        return v ^ (v >> 31);
    }
};
