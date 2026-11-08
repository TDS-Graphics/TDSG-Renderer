#pragma once
#include "geometry/bvh.h"
#include "light/lights.h"
#include "material/material.h"

class Scene {
public:
    int addMaterial(const Material& material);
    int addPrimitive(const Primitive& primitive);
    void addLight(std::shared_ptr<const Light> light);
    void Build();

    bool Intersect(const Ray& ray, Hit& hit) const;
    bool occluded(const Ray& ray) const;

    const Material& getMaterial(int id) const { return materials[id]; }
    const Primitive& getPrimitive(int id) const { return primitives[id]; }
    const Lights& getLights() const { return lights; }
    Spectrum background() const { return background_radiance; }
    void setBackground(Spectrum radiance) { background_radiance = radiance; }

    size_t primitiveCount() const { return primitives.size(); }

private:
    std::vector<Material> materials;
    std::vector<Primitive> primitives;
    Lights lights;
    BVH bvh;
    Spectrum background_radiance{0};
};
