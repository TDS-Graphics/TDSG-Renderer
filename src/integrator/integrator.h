#pragma once
#include <memory>
#include <string_view>
#include "scene/scene.h"

class Integrator {
public:
    virtual ~Integrator() = default;
    virtual Spectrum Li(const Ray& ray, const Scene& scene, Sampler& sampler) const = 0;
};

class PathIntegrator : public Integrator {
public:
    PathIntegrator(int max_depth = 8) : max_depth_(max_depth) {}
    Spectrum Li(const Ray& ray, const Scene& scene, Sampler& sampler) const override;
private:
    int max_depth_;
};

class WhittedIntegrator : public Integrator {
public:
    WhittedIntegrator(int max_depth = 8) : max_depth_(max_depth) {}
    Spectrum Li(const Ray& ray, const Scene& scene, Sampler& sampler) const override;
private:
    int max_depth_;
};

class NormalIntegrator : public Integrator {
public:
    Spectrum Li(const Ray& ray, const Scene& scene, Sampler&) const override;
};

std::unique_ptr<Integrator> makeIntegrator(std::string_view name, int max_depth);
