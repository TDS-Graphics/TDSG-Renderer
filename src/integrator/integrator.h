#pragma once
#include <memory>
#include <string_view>
#include "scene/scene.h"

class Integrator {
public:
    virtual ~Integrator() = default;
    // Radiance in the selected color backend; the color sample is shared by the path.
    virtual Spectrum Li(const Ray& ray, const Scene& scene, Sampler& sampler, const ColorSample& color_sample) const = 0;
};

class PathIntegrator : public Integrator {
public:
    PathIntegrator(int max_depth = 8) : max_depth_(max_depth) {}
    Spectrum Li(const Ray& ray, const Scene& scene, Sampler& sampler, const ColorSample& color_sample) const override;
private:
    int max_depth_;
};

class PathNEEIntegrator : public Integrator {
public:
    PathNEEIntegrator(int max_depth = 8) : max_depth_(max_depth) {}
    Spectrum Li(const Ray& ray, const Scene& scene, Sampler& sampler, const ColorSample& color_sample) const override;
private:
    int max_depth_;
};

class PathMISIntegrator : public Integrator {
public:
    PathMISIntegrator(int max_depth = 8) : max_depth_(max_depth) {}
    Spectrum Li(const Ray& ray, const Scene& scene, Sampler& sampler, const ColorSample& color_sample) const override;
private:
    int max_depth_;
};

class PhotonIntegrator : public Integrator {

};

class BDPTIntegrator : public Integrator {

};

class WhittedIntegrator : public Integrator {
public:
    WhittedIntegrator(int max_depth = 8) : max_depth_(max_depth) {}
    Spectrum Li(const Ray& ray, const Scene& scene, Sampler& sampler, const ColorSample& color_sample) const override;
private:
    int max_depth_;
};

class NormalIntegrator : public Integrator {
public:
    Spectrum Li(const Ray& ray, const Scene& scene, Sampler&, const ColorSample& color_sample) const override;
};

std::unique_ptr<Integrator> makeIntegrator(std::string_view name, int max_depth);
