#pragma once
#include <filesystem>
#include "scene/scene.h"

void LoadObj(Scene& scene, const std::filesystem::path& path,
              const Transform& transform = {}, int material_override = -1);
