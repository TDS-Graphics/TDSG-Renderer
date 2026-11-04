#include "integrator/integrator.h"
#include <array>
#include <utility>

// 路径追踪
Spectrum PathTracing(const Ray& ray, const Scene& scene, Sampler& sampler, int depth, int max_depth) {
    Hit hit;
    if (!scene.Intersect(ray, hit)) {
        return Spectrum(0);
    }
    Vec3f wo = -ray.direction;
    const Material& material = scene.getMaterial(hit.materialID);
    BSDFSample sampledBSDF = material.Sample(wo, hit.normal, hit.front_face, sampler);
    Spectrum radiance = material.emission;

    if (depth >= max_depth) {
        return radiance;
    }

    // 考虑采样到背面等情况
    if (sampledBSDF.pdf == 0 || maxComponent(sampledBSDF.weight) == 0) {
        return radiance;
    }

    Vec3f o = hit.position;
    Vec3f dir = sampledBSDF.direction;
    Ray next_ray(o, dir);
    return radiance + sampledBSDF.weight * PathTracing(next_ray, scene, sampler, depth + 1, max_depth);
}

// Whitted 光线追踪
Spectrum WhittedRayTracing(const Ray& ray, const Scene& scene, Sampler& sampler, int depth, int max_depth) {
    Hit hit;
    if (!scene.Intersect(ray, hit)) {
        return Spectrum(0);
    }

    Vec3f wo = -ray.direction;
    const Material& material = scene.getMaterial(hit.materialID);
    Spectrum radiance = material.emission;

    if (depth >= max_depth) {
        return radiance;
    }

    BSDFSample bsdf = material.Sample(wo, hit.normal, hit.front_face, sampler);
    if (bsdf.delta) {
        if (bsdf.pdf == 0 || maxComponent(bsdf.weight) == 0) {
            return radiance;
        }

        Ray next_ray(offsetOrigin(hit.position, hit.geometric_normal, bsdf.direction), bsdf.direction);
        return radiance + bsdf.weight * WhittedRayTracing(next_ray, scene, sampler, depth + 1, max_depth);
    }

    LightSample light = scene.getLights().sampleLight(hit.position, sampler);
    float cos_theta = std::max(0.0f, dot(hit.normal, light.direction));

    if (light.pdf > 0 && cos_theta > 0) {
        Vec3f origin = offsetOrigin(hit.position, hit.geometric_normal, light.direction);
        Vec3f to_light = light.position - origin;
        Ray shadow_ray(origin, to_light, 1e-4f,
                       length(to_light) - rayEpsilon(light.position));

        if (!scene.occluded(shadow_ray)) {
            radiance += material.eval(wo, light.direction, hit.normal)
                        * light.radiance
                        * (cos_theta / light.pdf);
        }
    }
    return radiance;
}

// 路径追踪 + MIS + NEE
// Spectrum PathTracing(const Ray& ray, const Scene& scene, Sampler& sampler,
//                      int depth, int max_depth, Vec3f previous_position = {},
//                      float previous_bsdf_pdf = 0, bool previous_delta = true) {
//     Hit hit;
//     if (!scene.Intersect(ray, hit)) {
//         return Spectrum(0);
//     }

//     Vec3f wo = -ray.direction;
//     const Material& material = scene.getMaterial(hit.materialID);
//     const Lights& lights = scene.getLights();
//     Spectrum radiance = lights.emission(hit, wo);

//     if (maxComponent(radiance) > EPS && !previous_delta) {
//         float light_pdf = lights.lightPdf(previous_position, hit);
//         radiance *= powerHeuristic(previous_bsdf_pdf, light_pdf);
//     }

//     if (depth >= max_depth) {
//         return radiance;
//     }

//     // Next-event estimation (NEE) 直接采样光源，和 BSDF 采样按 MIS 权重混合
//     if (!material.isDelta()) {
//         LightSample light = lights.sampleLight(hit.position, sampler);
//         Vec3f wi = light.direction;

//         float cos_theta = std::max(0.0f, dot(hit.normal, wi));

//         if (light.pdf > 0 && cos_theta > 0) {
//             Vec3f shadow_origin = offsetOrigin(hit.position, hit.geometric_normal,
//                                                light.position - hit.position);
//             Vec3f to_light = light.position - shadow_origin;

//             Ray shadow_ray(shadow_origin, to_light, 1e-4f,
//                            length(to_light) - rayEpsilon(light.position));

//             if (!scene.occluded(shadow_ray)) {
//                 float bsdf_pdf = material.pdf(wo, wi, hit.normal);

//                 // delta 散射事件的权重为1，因为方向确定
//                 float mis_weight = light.delta ? 1.0f : powerHeuristic(light.pdf, bsdf_pdf);

//                 radiance += material.eval(wo, wi, hit.normal)
//                             * light.radiance
//                             * (cos_theta * mis_weight / light.pdf);
//             }
//         }
//     }

//     BSDFSample bsdf = material.Sample(wo, hit.normal, hit.front_face, sampler);
//     if (bsdf.pdf == 0 || maxComponent(bsdf.weight) == 0) {
//         return radiance;
//     }

//     Vec3f origin = offsetOrigin(hit.position, hit.geometric_normal, bsdf.direction);
//     Ray next_ray(origin, bsdf.direction);
//     return radiance + bsdf.weight * PathTracing(next_ray,
//                                                scene,
//                                                sampler,
//                                                depth + 1,
//                                                max_depth,
//                                                hit.position,
//                                                bsdf.pdf,
//                                                bsdf.delta);
// }

Spectrum PathIntegrator::Li(const Ray& ray, const Scene& scene, Sampler& sampler) const {
    return PathTracing(ray, scene, sampler, 0, max_depth_);
}

Spectrum WhittedIntegrator::Li(const Ray& ray, const Scene& scene, Sampler& sampler) const {
    return WhittedRayTracing(ray, scene, sampler, 0, max_depth_);
}

Spectrum NormalIntegrator::Li(const Ray& ray, const Scene& scene, Sampler&) const {
    Hit hit;
    if (scene.Intersect(ray, hit)) {
        return (hit.normal + Spectrum(0.5)) * 0.5f;
    } else {
        return Spectrum(0);
    }
}

std::unique_ptr<Integrator> makeIntegrator(std::string_view name, int max_depth) {
    using Factory = std::unique_ptr<Integrator> (*)(int);
    static const std::array<std::pair<std::string_view, Factory>, 3> factories{{
        {"path", [](int depth) -> std::unique_ptr<Integrator> {
            return std::make_unique<PathIntegrator>(depth);
        }},
        {"whitted", [](int depth) -> std::unique_ptr<Integrator> {
            return std::make_unique<WhittedIntegrator>(depth);
        }},
        {"normal", [](int) -> std::unique_ptr<Integrator> {
            return std::make_unique<NormalIntegrator>();
        }},
    }};

    for (const auto& [factory_name, createIntegrator] : factories) {
        if (factory_name == name) {
            return createIntegrator(max_depth);
        }
    }

    return nullptr;
}
