#pragma once
#include <memory>
#include <optional>
#include <string_view>
#include "camera/camera.h"
#include "scene/scene.h"

struct SceneSetup {
    Scene scene;
    std::unique_ptr<Camera> camera;
};

SceneSetup MakeCornell();
SceneSetup MakeVeach();
std::optional<SceneSetup> makeScene(std::string_view name);

