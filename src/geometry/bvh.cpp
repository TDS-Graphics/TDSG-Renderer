#include "geometry/bvh.h"
#include <numeric>

void BVH::Build(const std::vector<Primitive>& primitives) {
    nodes.clear();
    indices.resize(primitives.size());
    std::iota(indices.begin(), indices.end(), 0);

    if (!primitives.empty()) {
        buildNode(primitives, 0, int(primitives.size()));
    }
}

int BVH::buildNode(const std::vector<Primitive>& primitives, int start, int end) {
    int index = int(nodes.size());
    nodes.push_back({});

    Bounds bounds, centroids;
    for (int i = start; i < end; ++i) {
        Bounds b = primitives[indices[i]].getBounds();
        bounds.expand(b);
        centroids.expand(b.center());
    }
    nodes[index].bounds = bounds;

    if (end - start <= 4) {
        nodes[index].start = start;
        nodes[index].count = end - start;
        return index;
    }

    Vec3f extent = centroids.upper - centroids.lower;
    int axis;
    if (extent.x > extent.y) {
        axis = 0;
    } else {
        axis = 1;
    }

    if (extent.z > extent[axis]) {
        axis = 2;
    }

    int middle = (start + end) / 2;
    std::nth_element(indices.begin() + start, indices.begin() + middle, indices.begin() + end,
                     [&](int a, int b) {
                         return primitives[a].getBounds().center()[axis] < primitives[b].getBounds().center()[axis];
                     });

    int left = buildNode(primitives, start, middle);
    int right = buildNode(primitives, middle, end);
    nodes[index].left = left;
    nodes[index].right = right;
    return index;
}

bool BVH::Intersect(const Ray& ray, const std::vector<Primitive>& primitives, Hit& hit, bool any_hit) const {
    return !nodes.empty() && intersectNode(0, ray, primitives, hit, any_hit);
}

bool BVH::intersectNode(int index, const Ray& ray, const std::vector<Primitive>& primitives,
                         Hit& hit, bool any_hit) const {
    const Node& node = nodes[index];
    if (!node.bounds.Intersect(ray, hit.t)) {
        return false;
    }

    bool found = false;
    if (node.count) {
        for (int i = node.start; i < node.start + node.count; ++i) {
            int id = indices[i];
            if (primitives[id].Intersect(ray, hit)) {
                hit.primitive = id;
                found = true;
                if (any_hit) {
                    return true;
                }
            }
        }
    } else {
        found = intersectNode(node.left, ray, primitives, hit, any_hit);
        if (found && any_hit) {
            return true;
        }

        found = intersectNode(node.right, ray, primitives, hit, any_hit) || found;
    }

    return found;
}
