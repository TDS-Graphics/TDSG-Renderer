#include "geometry/bvh.h"

// TinyBVH 1.9.1, upstream revision c9be3103cc9693b456b0a0353927c11cac7d710f.
// Custom geometry uses the portable BVH builder and traversal.
#define TINYBVH_NO_SIMD
#define TINYBVH_IMPLEMENTATION
#include "tinybvh/tiny_bvh.h"

namespace {

struct IntersectionQuery {
    const Ray& ray;
    const std::vector<Primitive>& primitives;
    Hit& hit;
    bool found = false;

    bool Intersect(uint32_t id) {
        if (!primitives[id].Intersect(ray, hit)) {
            return false;
        }
        hit.primitive = static_cast<int>(id);
        found = true;
        return true;
    }
};

} // namespace

struct BVH::Impl {
    tinybvh::BVH bvh;
};

void BVH::Build(const std::vector<Primitive>& primitives) {
    if (primitives.empty()) {
        impl.reset();
        return;
    }

    auto built = std::make_shared<Impl>();
    struct BuildContext {
        const std::vector<Primitive>& primitives;
    } context{primitives};
    built->bvh.customUserdata = &context;
    built->bvh.Build([](uint32_t id, tinybvh::bvhvec3& lower,
                       tinybvh::bvhvec3& upper, void* userdata) {
        const auto& primitives = static_cast<const BuildContext*>(userdata)->primitives;
        const Bounds bounds = primitives[id].getBounds();
        // Keep edge hits inside the bounds despite roundoff in slab traversal.
        const float scale = std::max({1.0f, std::abs(bounds.lower.x), std::abs(bounds.lower.y),
                                     std::abs(bounds.lower.z), std::abs(bounds.upper.x),
                                     std::abs(bounds.upper.y), std::abs(bounds.upper.z)});
        const float padding = 8 * std::numeric_limits<float>::epsilon() * scale;
        lower = tinybvh::bvhvec3(bounds.lower.x - padding, bounds.lower.y - padding,
                                bounds.lower.z - padding);
        upper = tinybvh::bvhvec3(bounds.upper.x + padding, bounds.upper.y + padding,
                                bounds.upper.z + padding);
    }, static_cast<uint32_t>(primitives.size()));
    built->bvh.customUserdata = nullptr;

    built->bvh.customIntersect = [](tinybvh::Ray& ray, uint32_t id, void*) {
        auto& query = *static_cast<IntersectionQuery*>(ray.hit.auxData);
        if (!query.Intersect(id)) {
            return false;
        }
        ray.hit.t = query.hit.t;
        ray.hit.prim = id;
        return true;
    };
    built->bvh.customIsOccluded = [](const tinybvh::Ray& ray, uint32_t id, void*) {
        return static_cast<IntersectionQuery*>(ray.hit.auxData)->Intersect(id);
    };
    impl = std::move(built);
}

bool BVH::Intersect(const Ray& ray, const std::vector<Primitive>& primitives,
                    Hit& hit, bool any_hit) const {
    const float limit = std::min(ray.t_far, hit.t);
    if (!impl || limit <= ray.t_near) {
        return false;
    }

    IntersectionQuery query{ray, primitives, hit};
    tinybvh::Ray bvh_ray(tinybvh::bvhvec3(ray.origin.x, ray.origin.y, ray.origin.z),
                         tinybvh::bvhvec3(ray.direction.x, ray.direction.y, ray.direction.z),
                         limit);
    // Per-ray state keeps concurrent traversal independent of shared BVH data.
    bvh_ray.hit.auxData = &query;
    if (any_hit) {
        return impl->bvh.IsOccluded(bvh_ray);
    }
    impl->bvh.Intersect(bvh_ray);
    return query.found;
}
