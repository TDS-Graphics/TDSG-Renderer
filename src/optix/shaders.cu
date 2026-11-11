// OptiX device programs for the GPU backend.
//
// One module holds every program: the ray generation kernel runs the whole path
// (iteratively) and uses optixTrace only to find the closest hit or to test
// occlusion, so all shading stays in a single translation unit.
//
// Compiled to PTX by CMake (see TDSG_OPTIX in the top level CMakeLists.txt).

#include <optix.h>
#include <optix_device.h>

#include "mathlib/math.h"
#include "core/ray.h"
#include "core/sampler.h"
#include "optix/device_scene.h"
#include "optix/device_shading.cuh"

extern "C" __constant__ DeviceLaunchParams params;

namespace dev {

struct DevHit {
    bool hit = false;
    float t = Infinity;
    int32_t primitive = -1;
    int32_t material = -1;
    int32_t light = -1;
    Vec3f position;
    Vec3f geometric_normal;
    Vec3f normal;
    bool front_face = true;
};

// Shadow rays stop at the first hit and run only the any-hit program.
__device__ inline bool occluded(Vec3f origin, Vec3f direction, float tmin, float tmax) {
    unsigned int result = 0;
    optixTrace(static_cast<OptixTraversableHandle>(params.handle),
               make_float3(origin.x, origin.y, origin.z),
               make_float3(direction.x, direction.y, direction.z),
               tmin, tmax, 0.0f, 0xFFu /*visibility mask*/,
               OPTIX_RAY_FLAG_TERMINATE_ON_FIRST_HIT | OPTIX_RAY_FLAG_DISABLE_CLOSESTHIT,
               0u, 0u, 0u, result);
    return result != 0;
}

__device__ inline bool intersectScene(Vec3f origin, Vec3f direction, float tmin, float tmax,
                                      DevHit& hit) {
    unsigned int p0 = 0xFFFFFFFFu, p1 = 0u, p2 = 0u, p3 = 0u, p4 = 0u;
    optixTrace(static_cast<OptixTraversableHandle>(params.handle),
               make_float3(origin.x, origin.y, origin.z),
               make_float3(direction.x, direction.y, direction.z),
               tmin, tmax, 0.0f, 0xFFu /*visibility mask*/,
               OPTIX_RAY_FLAG_DISABLE_ANYHIT,
               0u, 0u, 0u, p0, p1, p2, p3, p4);

    if (p0 == 0xFFFFFFFFu) {
        return false;
    }

    const DeviceSceneData& scene = params.scene;
    hit.hit = true;
    hit.t = __uint_as_float(p2);
    hit.primitive = int32_t(p0);
    hit.position = origin + hit.t * direction;

    if (p1 == 0u) {
        // Built-in triangles: reconstruct the CPU's Primitive::Intersect output.
        const DevVec3* p = scene.triVertices + size_t(p0) * 3;
        const Vec3f v0{p[0].x, p[0].y, p[0].z};
        const Vec3f v1{p[1].x, p[1].y, p[1].z};
        const Vec3f v2{p[2].x, p[2].y, p[2].z};
        const float u = __uint_as_float(p3);
        const float v = __uint_as_float(p4);
        const float w = 1 - u - v;

        hit.geometric_normal = normalize(cross(v1 - v0, v2 - v0));
        hit.front_face = dot(direction, hit.geometric_normal) < 0;

        const DeviceTriangleInfo& info = scene.triangles[p0];
        Vec3f ns = hit.geometric_normal;
        if (info.smooth) {
            ns = normalize(Vec3f{info.n0.x, info.n0.y, info.n0.z} * w +
                           Vec3f{info.n1.x, info.n1.y, info.n1.z} * u +
                           Vec3f{info.n2.x, info.n2.y, info.n2.z} * v);
        }
        if (dot(ns, hit.geometric_normal) < 0) {
            ns = -ns;
        }
        hit.normal = hit.front_face ? ns : -ns;
        hit.material = info.material;
        hit.light = info.light;
    } else {
        const DeviceSphere& sphere = scene.spheres[p0 - scene.triangleCount];
        const Vec3f center{sphere.center.x, sphere.center.y, sphere.center.z};
        hit.geometric_normal = normalize(hit.position - center);
        hit.front_face = dot(direction, hit.geometric_normal) < 0;
        hit.normal = hit.front_face ? hit.geometric_normal : -hit.geometric_normal;
        hit.material = sphere.material;
        hit.light = sphere.light;
    }

    return true;
}

// Mirrors PathTracing() in src/integrator/integrator.cpp.
__device__ inline DevSpectrum pathTrace(Vec3f origin, Vec3f direction, Sampler& sampler,
                                       const DevColorSample& cs) {
    const DeviceSceneData& scene = params.scene;
    DevSpectrum radiance = spectrum(0);
    DevSpectrum throughput = spectrum(1);

    for (int depth = 0; depth <= params.maxDepth; ++depth) {
        DevHit hit;
        if (!intersectScene(origin, direction, 1e-4f, 1e10f, hit)) {
            radiance += throughput * evaluate(cs, *scene.background);
            break;
        }

        const Vec3f wo = -direction;
        const DeviceMaterial& material = scene.materials[hit.material];
        // The CPU code samples the BSDF before the depth test.
        const DevBSDFSample bsdf =
            materialSample(material, wo, hit.normal, hit.front_face, sampler, cs);
        radiance += throughput * evaluate(cs, material.emission);

        if (depth >= params.maxDepth) {
            break;
        }
        if (bsdf.pdf == 0 || isZero(bsdf.weight)) {
            break;
        }

        throughput *= bsdf.weight;
        origin = hit.position;
        direction = bsdf.direction;
    }

    return radiance;
}

// Mirrors PathTracingNEE() in src/integrator/integrator.cpp.
__device__ inline DevSpectrum pathTraceNEE(Vec3f origin, Vec3f direction, Sampler& sampler,
                                          const DevColorSample& cs) {
    const DeviceSceneData& scene = params.scene;
    DevSpectrum radiance = spectrum(0);
    DevSpectrum throughput = spectrum(1);
    bool previous_delta = true;

    for (int depth = 0; depth <= params.maxDepth; ++depth) {
        DevHit hit;
        if (!intersectScene(origin, direction, 1e-4f, 1e10f, hit)) {
            radiance += throughput * evaluate(cs, *scene.background);
            break;
        }

        const Vec3f wo = -direction;
        const DeviceMaterial& material = scene.materials[hit.material];

        if (previous_delta) {
            radiance += throughput * lightEmission(scene, hit.light, hit.primitive,
                                                   hit.geometric_normal, wo, cs);
        }

        if (depth >= params.maxDepth) {
            break;
        }

        if (!isDelta(material)) {
            const DevLightSample light = sampleLight(scene, hit.position, sampler, cs);
            const float cos_theta = fmaxf(0.0f, dot(hit.normal, light.direction));

            if (light.pdf > 0 && cos_theta > 0) {
                const Vec3f shadow_origin =
                    offsetOrigin(hit.position, hit.geometric_normal, light.direction);
                // optixTrace measures t in units of the (here unnormalized)
                // direction, so normalize before passing world-space distances.
                const Vec3f to_light = light.position - shadow_origin;
                const float light_distance = length(to_light);
                const Vec3f shadow_direction = to_light / light_distance;

                if (!occluded(shadow_origin, shadow_direction, 1e-4f,
                              light_distance - rayEpsilon(light.position))) {
                    radiance += throughput *
                        (materialEval(material, wo, light.direction, hit.normal, cs) *
                         light.radiance * (cos_theta / light.pdf));
                }
            }
        }

        const DevBSDFSample bsdf =
            materialSample(material, wo, hit.normal, hit.front_face, sampler, cs);
        if (bsdf.pdf <= 0 || isZero(bsdf.weight)) {
            break;
        }

        origin = offsetOrigin(hit.position, hit.geometric_normal, bsdf.direction);
        direction = bsdf.direction;
        throughput *= bsdf.weight;
        previous_delta = bsdf.delta;
    }

    return radiance;
}

// Mirrors PathTracingMIS() in src/integrator/integrator.cpp.
__device__ inline DevSpectrum pathTraceMIS(Vec3f origin, Vec3f direction, Sampler& sampler,
                                          const DevColorSample& cs) {
    const DeviceSceneData& scene = params.scene;
    DevSpectrum radiance = spectrum(0);
    DevSpectrum throughput = spectrum(1);
    Vec3f previous_position{0, 0, 0};
    float previous_bsdf_pdf = 0;
    bool previous_delta = true;

    for (int depth = 0; depth <= params.maxDepth; ++depth) {
        DevHit hit;
        if (!intersectScene(origin, direction, 1e-4f, 1e10f, hit)) {
            radiance += throughput * evaluate(cs, *scene.background);
            break;
        }

        const Vec3f wo = -direction;
        const DeviceMaterial& material = scene.materials[hit.material];

        DevSpectrum surface =
            lightEmission(scene, hit.light, hit.primitive, hit.geometric_normal, wo, cs);
        if (!previous_delta) {
            float light_pdf = 0;
            if (hit.light >= 0 && hit.light < int32_t(scene.lightCount)) {
                light_pdf = scene.lights[hit.light].probability *
                    lightPdf(scene, scene.lights[hit.light], previous_position, hit.primitive,
                             hit.geometric_normal, hit.position);
            }
            surface = surface * powerHeuristic(previous_bsdf_pdf, light_pdf);
        }
        radiance += throughput * surface;

        if (depth >= params.maxDepth) {
            break;
        }

        if (!isDelta(material)) {
            const DevLightSample light = sampleLight(scene, hit.position, sampler, cs);
            const Vec3f wi = light.direction;
            const float cos_theta = fmaxf(0.0f, dot(hit.normal, wi));

            if (light.pdf > 0 && cos_theta > 0) {
                const Vec3f shadow_origin =
                    offsetOrigin(hit.position, hit.geometric_normal, light.position - hit.position);
                const Vec3f to_light = light.position - shadow_origin;
                const float light_distance = length(to_light);
                const Vec3f shadow_direction = to_light / light_distance;

                if (!occluded(shadow_origin, shadow_direction, 1e-4f,
                              light_distance - rayEpsilon(light.position))) {
                    const float bsdf_pdf = materialPdf(material, wo, wi, hit.normal);
                    const float mis_weight =
                        light.delta ? 1.0f : powerHeuristic(light.pdf, bsdf_pdf);

                    radiance += throughput *
                        (materialEval(material, wo, wi, hit.normal, cs) * light.radiance *
                         (cos_theta * mis_weight / light.pdf));
                }
            }
        }

        const DevBSDFSample bsdf =
            materialSample(material, wo, hit.normal, hit.front_face, sampler, cs);
        if (bsdf.pdf == 0 || isZero(bsdf.weight)) {
            break;
        }

        origin = offsetOrigin(hit.position, hit.geometric_normal, bsdf.direction);
        direction = bsdf.direction;
        throughput *= bsdf.weight;
        previous_position = hit.position;
        previous_bsdf_pdf = bsdf.pdf;
        previous_delta = bsdf.delta;
    }

    return radiance;
}

// Mirrors NormalIntegrator::Li().
__device__ inline DevSpectrum normalShade(Vec3f origin, Vec3f direction, const DevColorSample& cs) {
    DevHit hit;
    if (!intersectScene(origin, direction, 1e-4f, 1e10f, hit)) {
        return spectrum(0);
    }

    const DevVec3 color{(hit.normal.x + 1) * 0.5f, (hit.normal.y + 1) * 0.5f,
                        (hit.normal.z + 1) * 0.5f};
#if TDSG_SPECTRAL
    return evaluate(cs, fromRGBIlluminant(color, params.scene.illuminant, params.scene.cie));
#else
    DevColor scene_color;
    scene_color.v[0] = color.x;
    scene_color.v[1] = color.y;
    scene_color.v[2] = color.z;
    return evaluate(cs, scene_color);
#endif
}

}  // namespace dev

// ---------------------------------------------------------------------------
// Programs
// ---------------------------------------------------------------------------

extern "C" __global__ void __raygen__render() {
    const uint3 launch = optixGetLaunchIndex();
    const uint32_t x = launch.x;
    const uint32_t y = launch.y;
    if (x >= params.width || y >= params.height) {
        return;
    }

    const DeviceCamera& camera = params.camera;
    const float width = float(camera.width);
    const float height = float(camera.height);
    const float aspect = width / height;
    const Vec3f position{camera.position.x, camera.position.y, camera.position.z};
    const Vec3f forward{camera.forward.x, camera.forward.y, camera.forward.z};
    const Vec3f right{camera.right.x, camera.right.y, camera.right.z};
    const Vec3f up{camera.up.x, camera.up.y, camera.up.z};

    DevVec3 sum{0, 0, 0};
    for (uint32_t sample = 0; sample < params.spp; ++sample) {
        // Same stream as the CPU sampler, so seeds are reproducible.
        Sampler sampler = Sampler::forSample(params.seed, uint64_t(y) * params.width + x, sample);
        const Vec2 jitter = sampler.next2D();
        const Vec2 p_film{float(x) + jitter.x, float(y) + jitter.y};

        // Mirrors PinholeCamera::generateRay().
        const float sx = (2 * p_film.x / width - 1) * aspect * camera.tanHalfFov;
        const float sy = (1 - 2 * p_film.y / height) * camera.tanHalfFov;
        const Vec3f direction = normalize(forward + sx * right + sy * up);

        const dev::DevColorSample color_sample = dev::sampleColor(sampler);

        DevSpectrum radiance;
        switch (params.integrator) {
            case DEV_INTEGRATOR_NEE:
                radiance = dev::pathTraceNEE(position, direction, sampler, color_sample);
                break;
            case DEV_INTEGRATOR_MIS:
                radiance = dev::pathTraceMIS(position, direction, sampler, color_sample);
                break;
            case DEV_INTEGRATOR_NORMAL:
                radiance = dev::normalShade(position, direction, color_sample);
                break;
            default:
                radiance = dev::pathTrace(position, direction, sampler, color_sample);
                break;
        }

        const DevVec3 rgb = dev::toRGB(color_sample, radiance, params.scene.cie);
        sum.x += rgb.x;
        sum.y += rgb.y;
        sum.z += rgb.z;
    }

    params.accum[size_t(y) * params.width + x] = sum;
}

// Miss program: payload 0 stays 0xFFFFFFFF, which the ray generation program
// reads as "no hit" (the background is evaluated there).
extern "C" __global__ void __miss__background() {}

extern "C" __global__ void __closesthit__triangle() {
    const float2 bary = optixGetTriangleBarycentrics();
    optixSetPayload_0(optixGetPrimitiveIndex());
    optixSetPayload_1(0u);
    optixSetPayload_2(__float_as_uint(optixGetRayTmax()));
    optixSetPayload_3(__float_as_uint(bary.x));
    optixSetPayload_4(__float_as_uint(bary.y));
}

extern "C" __global__ void __closesthit__sphere() {
    optixSetPayload_0(params.scene.triangleCount + optixGetPrimitiveIndex());
    optixSetPayload_1(1u);
    optixSetPayload_2(__float_as_uint(optixGetRayTmax()));
    optixSetPayload_3(0u);
    optixSetPayload_4(0u);
}

// Analytic ray/sphere test, mirroring Primitive::Intersect() for spheres.
extern "C" __global__ void __intersection__sphere() {
    const DeviceSphere& sphere = params.scene.spheres[optixGetPrimitiveIndex()];
    const float3 origin = optixGetWorldRayOrigin();
    const float3 direction = optixGetWorldRayDirection();
    const float ox = origin.x - sphere.center.x;
    const float oy = origin.y - sphere.center.y;
    const float oz = origin.z - sphere.center.z;

    const float a = direction.x * direction.x + direction.y * direction.y + direction.z * direction.z;
    const float half_b = ox * direction.x + oy * direction.y + oz * direction.z;
    const float c = ox * ox + oy * oy + oz * oz - sphere.radius * sphere.radius;
    const float discriminant = half_b * half_b - a * c;
    if (discriminant < 0) {
        return;
    }

    const float root = sqrtf(discriminant);
    const float tmin = optixGetRayTmin();
    const float tmax = optixGetRayTmax();
    float t = (-half_b - root) / a;
    if (t <= tmin) {
        t = (-half_b + root) / a;
    }
    if (t <= tmin || t >= tmax) {
        return;
    }

    optixReportIntersection(t, 0u);
}

// Shadow rays only run this program (OPTIX_RAY_FLAG_DISABLE_CLOSESTHIT).
extern "C" __global__ void __anyhit__shadow() {
    optixSetPayload_0(1u);
    optixTerminateRay();
}

