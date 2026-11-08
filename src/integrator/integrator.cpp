#include "integrator/integrator.h"
#include <array>
#include <utility>

// 路径追踪
Spectrum PathTracing(const Ray& ray, const Scene& scene, Sampler& sampler, int depth, int max_depth) {
    Hit hit;
    if (!scene.Intersect(ray, hit)) {
        return scene.background();
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

    // 构造反射光线
    Vec3f o = hit.position;
    Vec3f dir = sampledBSDF.direction;
    Ray next_ray(o, dir);
    // 递归累加后续路径贡献
    return radiance + sampledBSDF.weight * PathTracing(next_ray, scene, sampler, depth + 1, max_depth);
}

// 路径追踪 + NEE
Spectrum PathTracingNEE(const Ray& ray, const Scene& scene, Sampler& sampler, int depth, int max_depth, bool previous_delta = true) {
    Hit hit;
    if (!scene.Intersect(ray, hit)) {
        return scene.background();
    }

    const Lights& lights = scene.getLights();
    const Material& material = scene.getMaterial(hit.materialID);

    Vec3f wo = -ray.direction;
    Spectrum radiance(0);

    // 自发光
    if (previous_delta) {
        radiance += lights.emission(hit, wo);
    }

    if (depth >= max_depth) {
        return radiance;
    }

    // NEE
    if (!material.isDelta()) {
        LightSample light = lights.sampleLight(hit.position, sampler);

        float cos_theta = std::max(
            0.0f, dot(hit.normal, light.direction)
        );

        if (light.pdf > 0 && cos_theta > 0) {
            Vec3f shadow_origin = offsetOrigin(hit.position, hit.geometric_normal, light.direction);
            Vec3f to_light = light.position - shadow_origin;
            Ray shadow_ray(shadow_origin, to_light, 1e-4f, length(to_light) - rayEpsilon(light.position));

            if (!scene.occluded(shadow_ray)) {
                radiance += material.eval(wo, light.direction, hit.normal)
                            * light.radiance
                            * (cos_theta / light.pdf);
            }
        }
    }

    // BSDF sampling
    BSDFSample bsdf = material.Sample(
        wo, hit.normal, hit.front_face, sampler
    );

    if (bsdf.pdf <= 0 || maxComponent(bsdf.weight) <= 0) {
        return radiance;
    }

    Vec3f next_origin = offsetOrigin(
        hit.position, hit.geometric_normal, bsdf.direction
    );

    Ray next_ray(next_origin, bsdf.direction);

    // 递归累加后续路径贡献
    return radiance + bsdf.weight * PathTracingNEE(next_ray,scene, sampler, depth + 1, max_depth, bsdf.delta);
}

// 路径追踪 + NEE + MIS
Spectrum PathTracingMIS(const Ray& ray, const Scene& scene, Sampler& sampler, int depth, int max_depth,
                        Vec3f previous_position = {0.f, 0.f, 0.f}, float previous_bsdf_pdf = 0, bool previous_delta = true) {
    Hit hit;
    if (!scene.Intersect(ray, hit)) {
        return scene.background();
    }

    Vec3f wo = -ray.direction;
    const Material& material = scene.getMaterial(hit.materialID);
    const Lights& lights = scene.getLights();
    Spectrum radiance = lights.emission(hit, wo);

    if (maxComponent(radiance) > EPS && !previous_delta) {
        float light_pdf = lights.lightPdf(previous_position, hit);
        radiance *= powerHeuristic(previous_bsdf_pdf, light_pdf);
    }

    if (depth >= max_depth) {
        return radiance;
    }

    // Next-event estimation (NEE) 直接采样光源，和 BSDF 采样按 MIS 权重混合
    if (!material.isDelta()) {
        LightSample light = lights.sampleLight(hit.position, sampler);
        Vec3f wi = light.direction;

        float cos_theta = std::max(0.0f, dot(hit.normal, wi));

        if (light.pdf > 0 && cos_theta > 0) {
            Vec3f shadow_origin = offsetOrigin(hit.position, hit.geometric_normal,
                                               light.position - hit.position);
            Vec3f to_light = light.position - shadow_origin;

            Ray shadow_ray(shadow_origin, to_light, 1e-4f,
                           length(to_light) - rayEpsilon(light.position));

            if (!scene.occluded(shadow_ray)) {
                float bsdf_pdf = material.pdf(wo, wi, hit.normal);

                // delta 散射事件的权重为1，因为方向确定
                float mis_weight = light.delta ? 1.0f : powerHeuristic(light.pdf, bsdf_pdf);

                radiance += material.eval(wo, wi, hit.normal)
                            * light.radiance
                            * (cos_theta * mis_weight / light.pdf);
            }
        }
    }

    BSDFSample bsdf = material.Sample(wo, hit.normal, hit.front_face, sampler);
    if (bsdf.pdf == 0 || maxComponent(bsdf.weight) == 0) {
        return radiance;
    }

    Vec3f origin = offsetOrigin(hit.position, hit.geometric_normal, bsdf.direction);
    Ray next_ray(origin, bsdf.direction);
    return radiance + bsdf.weight * PathTracingMIS(next_ray, scene, sampler, depth + 1, max_depth, hit.position, bsdf.pdf, bsdf.delta);
}


// Whitted-style Ray Tracing
Spectrum WhittedRayTracing(const Ray& ray, const Scene& scene, int depth, int max_depth) {
    Hit hit;
    if (!scene.Intersect(ray, hit)) {
        return scene.background();
    }

    Vec3f wo = -ray.direction;
    const Material& material = scene.getMaterial(hit.materialID);
    const Lights& lights = scene.getLights();
    Spectrum radiance = lights.emission(hit, wo);

    if (!material.isDelta()) {
        for (size_t i = 0; i < lights.size(); ++i) {
            const Light& source = lights.getLight(i);
            if (!source.isDelta()) {
                continue;
            }
            LightSample light = source.sample(hit.position, {});
            float cos_theta = std::max(0.0f, dot(hit.normal, light.direction));
            if (light.pdf <= 0 || cos_theta <= 0) {
                continue;
            }

            Vec3f origin = offsetOrigin(hit.position, hit.geometric_normal, light.direction);
            Vec3f to_light = light.position - origin;
            Ray shadow_ray(origin, to_light, 1e-4f,
                           length(to_light) - rayEpsilon(light.position));
            if (!scene.occluded(shadow_ray)) {
                radiance += material.eval(wo, light.direction, hit.normal)
                            * light.radiance * cos_theta;
            }
        }
    }

    // 深度上限只终止次级光线，当前表面的自发光和局部直接光仍然保留
    if (depth >= max_depth || maxComponent(material.color) <= 0) {
        return radiance;
    }

    auto trace = [&](Vec3f direction) {
        Ray next_ray(offsetOrigin(hit.position, hit.geometric_normal, direction), direction);
        return WhittedRayTracing(next_ray, scene, depth + 1, max_depth);
    };

    if (material.type == MaterialType::Mirror)
    {
        radiance += material.color * trace(reflect(wo, hit.normal));
    }
    else if (material.type == MaterialType::Glass)
    {
        float eta = hit.front_face ? 1 / material.ior : material.ior;
        float cos_i = std::clamp(dot(wo, hit.normal), 0.0f, 1.0f);
        float sin2_t = eta * eta * (1 - cos_i * cos_i);

        if (1 - sin2_t <= EPS) {
            // 全反射时没有折射
            radiance += material.color * trace(reflect(wo, hit.normal));
        } else {
            float cos_t = std::sqrt(1 - sin2_t);
            float fresnel = dielectricFresnel(cos_i, cos_t, eta);
            if (fresnel > 0) {
                radiance += material.color * fresnel * trace(reflect(wo, hit.normal));
            }
            if (fresnel < 1) {
                Vec3f direction = -eta * wo + (eta * cos_i - cos_t) * hit.normal;
                radiance += material.color * ((1 - fresnel) * eta * eta) * trace(direction);
            }
        }
    }
    return radiance;
}

Spectrum PathIntegrator::Li(const Ray& ray, const Scene& scene, Sampler& sampler) const {
    return PathTracing(ray, scene, sampler, 0, max_depth_);
}

Spectrum PathNEEIntegrator::Li(const Ray& ray, const Scene& scene, Sampler& sampler) const {
    return PathTracingNEE(ray, scene, sampler, 0, max_depth_);
}

Spectrum PathMISIntegrator::Li(const Ray& ray, const Scene& scene, Sampler& sampler) const {
    return PathTracingMIS(ray, scene, sampler, 0, max_depth_);
}

Spectrum WhittedIntegrator::Li(const Ray& ray, const Scene& scene, Sampler&) const {
    return WhittedRayTracing(ray, scene, 0, max_depth_);
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
    static const std::array<std::pair<std::string_view, Factory>, 5> factories{{
        {"path", [](int depth) -> std::unique_ptr<Integrator> {
            return std::make_unique<PathIntegrator>(depth);
        }},
        {"nee", [](int depth) -> std::unique_ptr<Integrator> {
            return std::make_unique<PathNEEIntegrator>(depth);
        }},
        {"mis", [](int depth) -> std::unique_ptr<Integrator> {
            return std::make_unique<PathMISIntegrator>(depth);
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
