#include "light/lights.h"
#include "light/area_light.h"
#include "scene/scene.h"
#include <algorithm>
#include <utility>

void Lights::addLight(std::shared_ptr<const Light> light) {
    registered_lights.push_back(std::move(light));
    lights.clear();
    primitive_lights.clear();
    light_weight = 0;
}

void Lights::Build(const Scene& scene) {
    lights.clear();
    primitive_lights.assign(scene.primitiveCount(), -1);
    light_weight = 0;

    for (const auto& light : registered_lights) {
        int primitiveID = light->getPrimitiveID();
        if (primitiveID >= 0) {
            primitive_lights[primitiveID] = int(lights.size());
        }
        lights.push_back({light});
    }

    // Preserve emissive materials from built-in scenes and OBJ/MTL imports.
    // Explicit area lights override that automatic registration.
    for (int i = 0; i < int(scene.primitiveCount()); ++i) {
        if (primitive_lights[i] != -1) {
            continue;
        }
        const Primitive& primitive = scene.getPrimitive(i);
        Spectrum emitted = scene.getMaterial(primitive.materialID).emission;
        if (primitive.area() > 0 && luminance(emitted) > 0) {
            primitive_lights[i] = int(lights.size());
            lights.push_back({std::make_shared<AreaLight>(i, primitive, emitted)});
        }
    }

    for (Entry& entry : lights) {
        float weight = luminance(entry.light->power());
        light_weight += weight;
        entry.cumulative_weight = light_weight;
    }
    if (light_weight > 0) {
        for (Entry& entry : lights) {
            entry.probability = float(luminance(entry.light->power()) / light_weight);
        }
    }
}

Spectrum Lights::emission(const Hit& hit, Vec3f wo) const {
    int index = primitive_lights[hit.primitive];
    if (index < 0) {
        return Spectrum(0);
    }
    return lights[index].light->evaluateEmission(hit, wo);
}

LightSample Lights::sampleLight(Vec3f from, Sampler& sampler) const {
    if (light_weight == 0) {
        return {};
    }

    double target = sampler.next1D() * light_weight;
    auto entry = std::upper_bound(lights.begin(), lights.end(), target,
                                 [](double value, const Entry& candidate) {
                                     return value < candidate.cumulative_weight;
                                 });
    LightSample result = entry->light->sample(from, sampler.next2D());
    result.pdf *= entry->probability;
    return result;
}

float Lights::lightPdf(Vec3f from, const Hit& light_hit) const {
    int index = primitive_lights[light_hit.primitive];
    if (index < 0) {
        return 0;
    }
    const Entry& entry = lights[index];
    return entry.probability * entry.light->pdf(from, light_hit);
}
