#include "scene/scene.h"
#include <utility>

int Scene::addMaterial(const Material& material) {
    materials.push_back(material);
    return int(materials.size()) - 1;
}

int Scene::addPrimitive(const Primitive& primitive) {
    primitives.push_back(primitive);
    return int(primitives.size()) - 1;
}

void Scene::addLight(std::shared_ptr<const Light> light) {
    lights.addLight(std::move(light));
}

void Scene::Build() {
    bvh.Build(primitives);
    lights.Build(*this);
}

bool Scene::Intersect(const Ray& ray, Hit& hit) const {
    return bvh.Intersect(ray, primitives, hit);
}

bool Scene::occluded(const Ray& ray) const {
    Hit hit;
    return bvh.Intersect(ray, primitives, hit, true);
}

