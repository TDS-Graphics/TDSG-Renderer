#pragma once
#include <cstdint>

// Plain-old-data scene description shared by the host uploader
// (src/optix/scene_upload.cpp) and the OptiX device programs
// (src/optix/shaders.cu). Only <cstdint> is used here so that the same header
// compiles with the C++ compiler and with nvcc.

#ifndef TDSG_SPECTRAL
#define TDSG_SPECTRAL 0
#endif

#if TDSG_SPECTRAL
// DenseSpectrum::SampleCount and a single sampled wavelength per path.
inline constexpr int DevColorSize = 32;
inline constexpr int DevSpectrumSize = 1;
#else
inline constexpr int DevColorSize = 3;
inline constexpr int DevSpectrumSize = 3;
#endif

struct DevVec2 {
    float x = 0, y = 0;
};

struct DevVec3 {
    float x = 0, y = 0, z = 0;
};

// Scene input color: 32 spectral samples or linear sRGB.
struct DevColor {
    float v[DevColorSize] = {};
};

// Radiance / BSDF weight carried along a path: one scalar in spectral mode,
// linear sRGB otherwise. Matches the CPU's Spectrum typedef.
struct DevSpectrum {
    float v[DevSpectrumSize] = {};
};

enum DevMaterialType : int32_t {
    DEV_MATERIAL_DIFFUSE = 0,
    DEV_MATERIAL_METAL = 1,
    DEV_MATERIAL_MIRROR = 2,
    DEV_MATERIAL_GLASS = 3,
};

enum DevLightType : int32_t {
    DEV_LIGHT_POINT = 0,
    DEV_LIGHT_SPOT = 1,
    DEV_LIGHT_AREA = 2,
};

enum DevIntegrator : int32_t {
    DEV_INTEGRATOR_PATH = 0,
    DEV_INTEGRATOR_NEE = 1,
    DEV_INTEGRATOR_MIS = 2,
    DEV_INTEGRATOR_NORMAL = 3,
};

struct DeviceMaterial {
    DevColor color;
    DevColor emission;
    float roughness = 0.3f;
    float ior = 1.5f;
    int32_t type = DEV_MATERIAL_DIFFUSE;
    int32_t pad = 0;
};

// Shading data of one triangle, indexed by the triangle's own id.
struct DeviceTriangleInfo {
    DevVec3 n0, n1, n2;
    int32_t material = 0;
    int32_t light = -1;
    int32_t smooth = 0;
};

struct DeviceSphere {
    DevVec3 center;
    float radius = 0;
    int32_t material = 0;
    int32_t light = -1;
};

// One entry of the light list, in the same order as Lights::Build.
struct DeviceLight {
    int32_t type = DEV_LIGHT_POINT;
    // Unified primitive id for area lights (triangles first, then spheres).
    int32_t primitive = -1;
    float probability = 0;
    int32_t pad0 = 0;
    double cumulative = 0;
    DevVec3 position;
    DevVec3 axis;
    float cosInner = 0;
    float cosOuter = 0;
    float area = 0;
    int32_t pad1 = 0;
    // Point/spot radiant intensity and area-light emitted radiance keep full
    // scene colors so spectral mode can evaluate them at the path wavelength.
    DevColor intensity;
    DevColor areaRadiance;
};

struct DeviceCamera {
    DevVec3 position;
    DevVec3 forward;
    DevVec3 right;
    DevVec3 up;
    float tanHalfFov = 0;
    int32_t width = 0;
    int32_t height = 0;
};

struct DeviceSceneData {
    const DevVec3* triVertices = nullptr;           // 3 vertices per triangle
    const DeviceTriangleInfo* triangles = nullptr;  // one per triangle
    const DeviceSphere* spheres = nullptr;
    const DeviceMaterial* materials = nullptr;
    const DeviceLight* lights = nullptr;
    const DevColor* background = nullptr;
    // Spectral mode only: 32x3 CIE curves and the 8x32 Smits illuminant basis.
    const float* cie = nullptr;
    const float* illuminant = nullptr;
    uint32_t triangleCount = 0;
    uint32_t sphereCount = 0;
    uint32_t lightCount = 0;
    double lightWeight = 0;
};

struct DeviceLaunchParams {
    DeviceSceneData scene;
    DeviceCamera camera;
    DevVec3* accum = nullptr;  // per-pixel sum of the RGB samples
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t spp = 1;
    uint64_t seed = 1;
    int32_t maxDepth = 8;
    int32_t integrator = DEV_INTEGRATOR_PATH;
    // OptixTraversableHandle is an unsigned 64-bit handle; kept as a plain
    // integer so this header stays free of OptiX/CUDA headers.
    uint64_t handle = 0;
};
