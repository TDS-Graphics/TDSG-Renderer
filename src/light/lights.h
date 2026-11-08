#pragma once
#include <memory>
#include <vector>
#include "core/sampler.h"
#include "light/light.h"

class Scene;

class Lights {
public:
    void addLight(std::shared_ptr<const Light> light);
    void Build(const Scene& scene);

    Spectrum emission(const Hit& hit, Vec3f wo) const;
    LightSample sampleLight(Vec3f from, Sampler& sampler) const;
    float lightPdf(Vec3f from, const Hit& light_hit) const;
    size_t size() const { return lights.size(); }
    const Light& getLight(size_t index) const { return *lights[index].light; }

private:
    struct Entry {
        std::shared_ptr<const Light> light;
        float probability = 0;
        double cumulative_weight = 0;
    };

    // Light objects own their data and expose const operations, so Scene copies
    // can safely share them without retaining pointers into either Scene.
    std::vector<std::shared_ptr<const Light>> registered_lights;
    std::vector<Entry> lights;
    std::vector<int> primitive_lights;
    double light_weight = 0;
};
