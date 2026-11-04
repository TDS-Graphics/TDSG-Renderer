#include "scene/obj_loader.h"
#include <iostream>
#include <stdexcept>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

void LoadObj(Scene& scene, const std::filesystem::path& path, const Transform& transform, int material_override) {
    tinyobj::ObjReaderConfig config;
    config.triangulate = true;
    config.mtl_search_path = path.parent_path().string();

    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(path.string(), config)) {
        throw std::runtime_error(reader.Error());
    }

    if (!reader.Warning().empty()) {
        std::cerr << reader.Warning();
    }

    std::vector<int> materials;
    bool has_textures = false;
    for (const auto& source : reader.GetMaterials()) {
        Material m;
        m.color = {source.diffuse[0], source.diffuse[1], source.diffuse[2]};
        m.emission = {source.emission[0], source.emission[1], source.emission[2]};
        if (source.illum == 5 || source.metallic > 0) {
            m.type = MaterialType::Metal;
            m.color = {source.specular[0], source.specular[1], source.specular[2]};
            if (source.roughness > 0) {
                m.roughness = source.roughness;
            } else {
                m.roughness = std::sqrt(2 / (source.shininess + 2));
            }
        } else if (source.illum == 7) {
            m.type = MaterialType::Glass;
            m.color = Spectrum(1);
            m.ior = source.ior;
        }

        materials.push_back(scene.addMaterial(m));
        has_textures = has_textures || !source.diffuse_texname.empty();
    }
    if (has_textures) {
        std::cerr << "OBJ: texture maps are not loaded; using MTL constant colors.\n";
    }

    int fallback;
    if (material_override >= 0) {
        fallback = material_override;
    } else {
        fallback = scene.addMaterial(Material{});
    }

    const auto& attrib = reader.GetAttrib();
    for (const auto& shape : reader.GetShapes()) {
        size_t offset = 0;
        for (size_t face = 0; face < shape.mesh.num_face_vertices.size(); ++face) {
            Triangle triangle;
            triangle.smooth = true;

            for (int v = 0; v < 3; ++v) {
                auto index = shape.mesh.indices[offset + v];
                size_t p = size_t(index.vertex_index) * 3;
                triangle.positions[v] = transform.point({attrib.vertices[p], attrib.vertices[p + 1], attrib.vertices[p + 2]});
                if (index.normal_index >= 0) {
                    size_t n = size_t(index.normal_index) * 3;
                    triangle.normals[v] = transform.normal({attrib.normals[n], attrib.normals[n + 1], attrib.normals[n + 2]});
                } else {
                    triangle.smooth = false;
                }
            }

            offset += 3;
            int source_material = shape.mesh.material_ids[face];
            int material;
            if (material_override >= 0) {
                material = material_override;
            } else if (source_material >= 0) {
                material = materials[source_material];
            } else {
                material = fallback;
            }

            const auto& p = triangle.positions;
            if (lengthSquared(cross(p[1] - p[0], p[2] - p[0])) == 0) {
                continue;
            }

            scene.addPrimitive({triangle, material});
        }
    }
}
