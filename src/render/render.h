#pragma once
#include <thread>
#include "camera/camera.h"
#include "integrator/integrator.h"
#include "film/film.h"

struct RenderSettings {
    int spp = 4, max_depth = 8;
    int threads = int(std::max(1u, std::thread::hardware_concurrency()));
    uint64_t seed = 1;
};

void Render(const Scene& scene, const Camera& camera, Film& film,
            const Integrator& integrator, const RenderSettings& settings);