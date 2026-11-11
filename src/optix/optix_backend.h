#pragma once
#include <string>
#include <string_view>
#include "camera/camera.h"
#include "film/film.h"
#include "render/render.h"
#include "scene/scene.h"

// True when a CUDA device is present and the OptiX backend was compiled in.
bool OptixAvailable();

// Renders the scene with the OptiX backend. Supported integrators: path, nee,
// mis, normal. Returns false and fills `error` when the backend cannot run.
bool RenderOptix(const Scene& scene, const Camera& camera, Film& film,
                 std::string_view integrator, const RenderSettings& settings,
                 int device, std::string& error);
