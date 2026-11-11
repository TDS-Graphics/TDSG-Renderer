#pragma once
#include <vector>
#include "optix/device_scene.h"
#include "scene/scene.h"

// Host-side copy of a Scene in the layout expected by the OptiX device
// programs. Triangles are numbered first, spheres follow, and the light list
// keeps the order produced by Scene::Build() so the GPU selects the same light
// as the CPU for a given random number.
struct UploadedScene {
    std::vector<DevVec3> triVertices;  // 3 vertices per triangle
    std::vector<DeviceTriangleInfo> triangles;
    std::vector<DeviceSphere> spheres;
    std::vector<DeviceMaterial> materials;
    std::vector<DeviceLight> lights;
    // Spectral mode only: device copies of the color matching tables.
    std::vector<float> cie;
    std::vector<float> illuminant;
    DevColor background;
    double lightWeight = 0;

    uint32_t triangleCount() const { return uint32_t(triangles.size()); }
    uint32_t sphereCount() const { return uint32_t(spheres.size()); }
};

UploadedScene UploadScene(const Scene& scene);
