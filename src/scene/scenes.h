#pragma once
#include <memory>
#include "camera/camera.h"
#include "scene/scene.h"

struct SceneSetup {
    Scene scene;
    std::unique_ptr<Camera> camera;
    int width, height;
};

SceneSetup MakeCornell();
SceneSetup MakeVeach();
