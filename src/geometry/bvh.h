#pragma once
#include <memory>
#include <vector>
#include "geometry/primitive.h"

class BVH {
public:
    void Build(const std::vector<Primitive>& primitives);
    bool Intersect(const Ray& ray, const std::vector<Primitive>& primitives, Hit& hit, bool any_hit = false) const;

private:
    struct Impl;
    std::shared_ptr<const Impl> impl;
};
