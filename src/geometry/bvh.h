#pragma once
#include <vector>
#include "geometry/primitive.h"

class BVH {
public:
    void Build(const std::vector<Primitive>& primitives);
    bool Intersect(const Ray& ray, const std::vector<Primitive>& primitives, Hit& hit, bool any_hit = false) const;

private:
    struct Node {
        Bounds bounds;
        int left = -1, right = -1, start = 0, count = 0;
    };

    std::vector<Node> nodes;
    std::vector<int> indices;

    int buildNode(const std::vector<Primitive>& primitives, int start, int end);
    bool intersectNode(int node, const Ray& ray, const std::vector<Primitive>& primitives,
                       Hit& hit, bool any_hit) const;
};
