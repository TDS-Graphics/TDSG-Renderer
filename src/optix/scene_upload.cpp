#include "optix/scene_upload.h"
#include <algorithm>
#include <variant>
#include "light/area_light.h"
#include "light/point_light.h"
#include "light/spot_light.h"
#if TDSG_SPECTRAL
#include "color/smits_data.h"
#include "color/spectrum_data.h"
#endif

namespace {

DevColor toDevColor(const Color& color) {
    DevColor result;
#if TDSG_SPECTRAL
    for (int i = 0; i < DevColorSize; ++i) {
        result.v[i] = color[i];
    }
#else
    for (int i = 0; i < 3; ++i) {
        result.v[i] = color[i];
    }
#endif
    return result;
}

DevVec3 toDevVec3(Vec3f v) { return {v.x, v.y, v.z}; }

int32_t devMaterialType(MaterialType type) {
    switch (type) {
        case MaterialType::Metal:
            return DEV_MATERIAL_METAL;
        case MaterialType::Mirror:
            return DEV_MATERIAL_MIRROR;
        case MaterialType::Glass:
            return DEV_MATERIAL_GLASS;
        case MaterialType::Diffuse:
        default:
            return DEV_MATERIAL_DIFFUSE;
    }
}

}  // namespace

UploadedScene UploadScene(const Scene& scene) {
    UploadedScene uploaded;

    // Materials are compacted to the ones actually referenced by primitives so
    // the device array stays small and densely indexed.
    std::vector<int> materialSource;
    const auto materialIndex = [&](int sourceId) {
        for (size_t i = 0; i < materialSource.size(); ++i) {
            if (materialSource[i] == sourceId) {
                return int(i);
            }
        }

        materialSource.push_back(sourceId);
        const Material& material = scene.getMaterial(sourceId);
        DeviceMaterial converted;
        converted.type = devMaterialType(material.type);
        converted.color = toDevColor(material.color);
        converted.emission = toDevColor(material.emission);
        converted.roughness = material.roughness;
        converted.ior = material.ior;
        uploaded.materials.push_back(converted);
        return int(uploaded.materials.size()) - 1;
    };

    const size_t primitiveCount = scene.primitiveCount();
    std::vector<int> unifiedId(primitiveCount, -1);
    std::vector<int> sphereSlot(primitiveCount, -1);

    uploaded.triVertices.reserve(primitiveCount * 3);
    for (size_t i = 0; i < primitiveCount; ++i) {
        const Primitive& primitive = scene.getPrimitive(int(i));
        const int material = materialIndex(primitive.materialID);
        const int32_t triangleCount = int32_t(uploaded.triVertices.size() / 3);

        if (const auto* sphere = std::get_if<Sphere>(&primitive.shape)) {
            DeviceSphere converted;
            converted.center = toDevVec3(sphere->center);
            converted.radius = sphere->radius;
            converted.material = material;
            converted.light = -1;
            sphereSlot[i] = int(uploaded.spheres.size());
            uploaded.spheres.push_back(converted);
            continue;
        }

        const Triangle& triangle = std::get<Triangle>(primitive.shape);
        DeviceTriangleInfo info;
        info.n0 = toDevVec3(triangle.normals[0]);
        info.n1 = toDevVec3(triangle.normals[1]);
        info.n2 = toDevVec3(triangle.normals[2]);
        info.material = material;
        info.light = -1;
        info.smooth = triangle.smooth ? 1 : 0;
        uploaded.triangles.push_back(info);
        for (Vec3f position : triangle.positions) {
            uploaded.triVertices.push_back(toDevVec3(position));
        }
        unifiedId[i] = int(triangleCount);
    }

    // Spheres are numbered after all triangles, matching the shader's mapping.
    const int32_t triangleCount = int32_t(uploaded.triangles.size());
    for (size_t i = 0; i < primitiveCount; ++i) {
        if (sphereSlot[i] >= 0) {
            unifiedId[i] = triangleCount + sphereSlot[i];
        }
    }

    // Lights: the final list (registered + auto-registered emissive primitives)
    // as built by Lights::Build().
    const Lights& lights = scene.getLights();
    std::vector<int> primitiveLight(primitiveCount, -1);
    uploaded.lights.reserve(lights.size());

    for (size_t i = 0; i < lights.size(); ++i) {
        const Light& light = lights.getLight(i);
        DeviceLight converted;

        if (const int primitiveID = light.getPrimitiveID(); primitiveID >= 0) {
            const Primitive& primitive = scene.getPrimitive(primitiveID);
            converted.type = DEV_LIGHT_AREA;
            converted.primitive = unifiedId[primitiveID];
            converted.area = primitive.area();
            if (const auto* area = dynamic_cast<const AreaLight*>(&light)) {
                converted.areaRadiance = toDevColor(area->getRadiance());
            } else {
                converted.areaRadiance =
                    toDevColor(scene.getMaterial(primitive.materialID).emission);
            }
            primitiveLight[primitiveID] = int(i);
        } else if (const auto* point = dynamic_cast<const PointLight*>(&light)) {
            converted.type = DEV_LIGHT_POINT;
            converted.position = toDevVec3(point->getPosition());
            converted.intensity = toDevColor(point->getIntensity());
        } else if (const auto* spot = dynamic_cast<const SpotLight*>(&light)) {
            converted.type = DEV_LIGHT_SPOT;
            converted.position = toDevVec3(spot->getPosition());
            converted.axis = toDevVec3(normalize(spot->getDirection()));
            converted.intensity = toDevColor(spot->getIntensity());
            converted.cosInner = spot->getCosInner();
            converted.cosOuter = spot->getCosOuter();
        }

        uploaded.lights.push_back(converted);
    }

    // Discrete light selection weights, mirroring Lights::Build().
    double lightWeight = 0;
    for (size_t i = 0; i < uploaded.lights.size(); ++i) {
        lightWeight += luminance(lights.getLight(i).power());
        uploaded.lights[i].cumulative = lightWeight;
    }
    if (lightWeight > 0) {
        for (size_t i = 0; i < uploaded.lights.size(); ++i) {
            uploaded.lights[i].probability =
                float(luminance(lights.getLight(i).power()) / lightWeight);
        }
    }
    uploaded.lightWeight = lightWeight;

    // Attach the light index to every primitive so hit shading matches
    // Lights::emission()/lightPdf().
    for (size_t i = 0; i < primitiveCount; ++i) {
        const int id = unifiedId[i];
        if (id < 0) {
            continue;
        }
        if (id < triangleCount) {
            uploaded.triangles[id].light = primitiveLight[i];
        } else {
            uploaded.spheres[id - triangleCount].light = primitiveLight[i];
        }
    }

    uploaded.background = toDevColor(scene.background());

#if TDSG_SPECTRAL
    // Constexpr host tables are not readable from device code, so they travel
    // with the scene.
    uploaded.cie.assign(&color_detail::CIE[0][0], &color_detail::CIE[0][0] + 32 * 3);
    uploaded.illuminant.assign(&color_detail::RGBIlluminant[0][0],
                               &color_detail::RGBIlluminant[0][0] + 8 * 32);
#endif

    return uploaded;
}

